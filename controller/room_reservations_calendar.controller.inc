<?php
/**
 * @file
 * Calendar controller functionality.
 */

/**
 * Display the reservation calendar.
 *
 * @param int $selected_month
 *   (optional) The month of the day selected by the user.
 * @param int $selected_day
 *   (optional) The day of the month selected by the user.
 * @param string $selected_category
 *   (optional) The room category selected by the user.
 *
 * @return string
 *   The page display formatted as HTML.
 */
function room_reservations($selected_month = NULL, $selected_day = NULL, $selected_category = NULL) {
  $dates = _room_reservations_dates($selected_month, $selected_day);
  
  // Determine which day has been selected by the user. If the user has entered a url that specifies a day outside of the
  // allowable reservation window, then set the current day as the selected day.
  $yyyymmdd = $dates[0]['yyyymmdd'];
  foreach ($dates as $day) {
    if ($day['selected']) {
      $yyyymmdd = $day['yyyymmdd'];
    }
  }
  if ($yyyymmdd == $dates[0]['yyyymmdd']) {
    $dates[0]['selected'] = TRUE;
    $dates[0]['today'] = TRUE;
  }
  $categories = _room_reservations_categories();
  
  // If the day being displayed is today, only display time slots that are
  // later than the current time minus two hours.
  $today_displayed = FALSE;
  foreach ($dates as $day) {
    if (($day['selected']) && $day['today']) {
      $today_displayed = TRUE;
    }
  }
  if ($today_displayed) {
    $hours = _room_reservations_hours('limited');
  }
  else {
    $hours = _room_reservations_hours();
  }
  $_room_reservations_building_hours = _room_reservations_facility_hours();
  $display_hours = _room_reservations_hours();
  
  // Determine the open hours (display version) for the date selected by the user.
  $building_hours_day = $_room_reservations_building_hours[$yyyymmdd];
  $building_hours_display = $building_hours_day['display'];
  
  // For each time slot, determine if the rooms are open or closed.
  $building_hours_day = $_room_reservations_building_hours[$yyyymmdd];
  $int_first_shift_open = intval($building_hours_day['first_shift_open']);
  $int_first_shift_close = intval($building_hours_day['first_shift_close']);
  $int_second_shift_open = intval($building_hours_day['second_shift_open']);
  $int_second_shift_close = intval($building_hours_day['second_shift_close']);
  $open_24 = $building_hours_day['open_24_hours'];
  $x = 0;
  foreach ($hours as $time_slot) {
    $int_time_slot_time = intval($time_slot['time']);
    if ($building_hours_day['open_24_hours']) {
      $time_slot['open'] = TRUE;
    }
    elseif ((($int_time_slot_time >= $int_first_shift_open) && ($int_time_slot_time < $int_first_shift_close)) || 
      (($int_time_slot_time >= $int_second_shift_open) && ($int_time_slot_time < $int_second_shift_close))) {
      $time_slot['open'] = TRUE;
    }
    else {
      $time_slot['open'] = FALSE;
    }
    
    // if not open ALL day let's limit display to start just before first open shift (or 2nd if only one used)
    $buffer = variable_get('room_reservations_before_after_hours', 3) * 100;
    if (!$open_24 && $buffer != 300 && $int_first_shift_open < 9999 && ($hours[$x]['time'] < $int_first_shift_open - $buffer)) {
      unset($hours[$x]);
    }
    elseif (!$open_24 && $buffer != 300 && $int_second_shift_open < 9999 && ($hours[$x]['time'] < $int_second_shift_open - $buffer)) {
      unset($hours[$x]);
    }
    else {
      $hours[$x] = $time_slot;
    }
    // and do the same for closing
    if (!$open_24 && $buffer != 300 && $int_second_shift_close < 9999 && ($hours[$x]['time'] >= $int_second_shift_close + $buffer)) {
      unset($hours[$x]);
    }
    elseif (!$open_24 && $buffer != 300 && $int_first_shift_close < 9999 && ($hours[$x]['time'] >= $int_first_shift_close + $buffer)) {
      unset($hours[$x]);
    }
    
    $x++;
  }
  $all_hours = _room_reservations_hours();
  $times = _room_reservations_times();
  $rooms = _room_reservations_rooms();
  
  // Initialize the $reservations array.
  $reservations = array();
  foreach ($rooms as $room) {
    $room_name = $room['name'];
    foreach ($hours as $time_slot) {
      $time = $time_slot['time'];
      $reservations[$room['nid']][$time] = array(
        'time' => $time,
        'display' => $time_slot['display'],
        'class' => $time_slot['class'],
        'id' => '',
        'booked' => FALSE,
        'start' => FALSE,
        'end' => FALSE,
        'user' => '',
        'name' => '',
      );
    }
  }
  
  $query = new EntityFieldQuery();
  $query->entityCondition('entity_type', 'node')
    ->entityCondition('bundle', 'room_reservations_reservation')
    ->fieldCondition('reservation_date', 'value', $yyyymmdd . '%', 'like')
    ->fieldOrderBy('reservation_time', 'value', 'DESC');
  $result = $query->execute();
  if (isset($result['node'])) {
    $result_nids = array_keys($result['node']);
    $results = entity_load('node', $result_nids);
    foreach ($results as $data) {
      $id = $data->nid;
      $time = $data->reservation_time['und'][0]['value'];;
      $rid = $data->reservation_room['und'][0]['target_id'];
      $name = $data->title;
      $user_name = $data->name;
      $reservations[$rid][$time]['booked'] = TRUE;
      $reservations[$rid][$time]['name'] = $name;
      $reservations[$rid][$time]['user_name'] = $user_name;
      $reservations[$rid][$time]['id'] = $id; 
      $reservations[$rid][$time]['series_id'] = isset($data->reservation_series_id['und'][0]['value']) ? $data->reservation_series_id['und'][0]['value'] : '';
      $reservations[$rid][$time]['user'] = $data->uid;
      $reservations[$rid][$time]['blocked'] = $data->reservation_block_title['und'][0]['value'];
      $length = $data->reservation_length['und'][0]['value'];;
      $time_slots = $length / 30;
      $reservations[$rid][$time]['slots'] = $time_slots;
      $remainder = $length % 30;
      if ($remainder > 1) {
        $time_slots++;
      }
      $key = array_search($time, $times);
      $x = 2;
      while ($x <= $time_slots) {
        $key++;
        $next_time = $times[$key];
        $reservations[$rid][$next_time]['booked'] = TRUE;
        $x++;
      }
    }
  }
    
  // Get any room_reservation records for the previous day
  // that might carry over into the selected day.
  // Continue to update the $reservation array with that information.
  $previous_day = date('Y-m-d', strtotime("$yyyymmdd -1 days"));
  $max_length = user_access('create room reservations extended length') ? variable_get('room_reservations_max_length_extended', 120) : variable_get('room_reservations_max_length_standard', 120);
  $lhours = _room_reservations_hours();
  $rhours = array_reverse($lhours);
  $slots = ($max_length / 30) - 1;
  $late_times = array();
  $search_times = array(
    '9999',
    '9999',
    '9999',
    '9999',
    '9999',
    '9999',
    '9999',
  );
  for ($j = 0; $j < $slots; $j++) {
    $hour = $rhours[$j];
    $late_times[] = $hour['time'];
    $search_times[$j] = $hour['time'];
  }
  $late_times = array_reverse($late_times);
  
  $query = new EntityFieldQuery();
  $query->entityCondition('entity_type', 'node')
    ->entityCondition('bundle', 'room_reservations_reservation')
    ->fieldCondition('reservation_date', 'value', $previous_day . '%', 'like')
    ->fieldCondition('reservation_time', 'value', $search_times,  'in')
    ->fieldOrderBy('reservation_time', 'value', 'DESC');
  $result = $query->execute();
  if (isset($result['node'])) {
    $result_nids = array_keys($result['node']);
    $results = entity_load('node', $result_nids);
    foreach ($results as $data) {
      $id = $data->id;
      $time = $data->time;
      $length = $data->length;
      $room_name = $data->room;
      $name = $data->name;
      $user_name = $data->user_name;
      $time_slots = $length / 30;
      $remainder = $length % 30;
      if ($remainder > 1) {
        $time_slots++;
      }
      for ($j = 0; $j < (($max_length / 30) - 1); $j++) {
        if ($late_times[$j] == $time) {
          $carry_over_time_slots = ($time_slots + $j - (($max_length / 30) - 1));
          break;
        }
      }
      if ($carry_over_time_slots > 0) {
        $reservations[$rid]['0000']['booked'] = TRUE;
        $reservations[$rid]['0000']['name'] = $name;
        $reservations[$rid]['0000']['user_name'] = $user_name;
        $reservations[$rid]['0000']['id'] = $id;
        $carry_over_time_slots--;
      }
      while ($carry_over_time_slots > 0) {
        $next_time = $times[$carry_over_time_slots];
        $reservations[$rid][$next_time]['booked'] = TRUE;
        $carry_over_time_slots--;
      }
    }
  }
  $user_reservations = _room_reservations_user_reservations();
  $output = '';
  $output .= theme('room_reservations', array(
    'dates' => $dates,
    'categories' => $categories,
    'reservations' => $reservations, 
    'hours' => $hours,
    'building_hours' => $_room_reservations_building_hours,
    'building_hours_display' => $building_hours_display,
    'rooms' => $rooms, 
    'selected_category' => $selected_category,
    'user_reservations' => $user_reservations));
  return $output;
}
