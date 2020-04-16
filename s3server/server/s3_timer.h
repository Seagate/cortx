/*
 * COPYRIGHT 2016 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.seagate.com/contact
 *
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 7-Jan-2016
 */

#pragma once

#ifndef __S3_SERVER_S3_TIMER_H__
#define __S3_SERVER_S3_TIMER_H__

#include <chrono>

// USAGE:
// S3Timer timer;
// timer.start();
// Do some operations to be timed
// timer.stop();
// Do some other operations
// timer.resume();
// Do some operations to be timed
// timer.stop();
// cout << "Elapsed time (ms) = " << timer.elapsed_time_in_millisec() << endl;
// cout << "Elapsed time (nanosec) = " << timer.elapsed_time_in_nanosec() <<
// endl << endl;
//

class S3Timer {
  enum class S3TimerState {
    unknown,
    started,
    stopped
  };

  using Clock = std::chrono::steady_clock;
  using Duration = Clock::duration;

  std::chrono::time_point<Clock> start_time;
  std::chrono::time_point<Clock> end_time;
  Duration duration;
  S3TimerState state = S3TimerState::unknown;

 public:

  void start() {
    start_time = Clock::now();
    state = S3TimerState::started;
    duration = Duration(0);
  }

  void stop() {
    if (state == S3TimerState::started) {
      end_time = Clock::now();
      state = S3TimerState::stopped;
      duration += end_time - start_time;
    } else {
      state = S3TimerState::unknown;
    }
  }

  void resume() {
    if (state == S3TimerState::stopped) {
      start_time = Clock::now();
      state = S3TimerState::started;
    } else {
      state = S3TimerState::unknown;
    }
  }

  std::chrono::milliseconds::rep elapsed_time_in_millisec() const {
    if (state == S3TimerState::stopped) {
      return std::chrono::duration_cast<std::chrono::milliseconds>(duration)
          .count();
    }
    return -1;
  }

  std::chrono::nanoseconds::rep elapsed_time_in_nanosec() const {
    if (state == S3TimerState::stopped) {
      return std::chrono::duration_cast<std::chrono::nanoseconds>(duration)
          .count();
    }
    return -1;
  }
};

#endif
