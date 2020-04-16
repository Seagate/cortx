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
 * Original author:  Rajesh Nambiar   <rajesh.nambiar@seagate.com>
 * Original creation date: 12-April-2016
 */

#include "s3_daemonize_server.h"
#include <evhtp.h>
#include <execinfo.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include "clovis_helpers.h"
#include "s3_iem.h"
#include "s3_log.h"
#include "s3_option.h"

#define S3_BACKTRACE_DEPTH_MAX 2048
#define S3_ARRAY_SIZE(a) ((sizeof(a)) / (sizeof(a)[0]))

extern evbase_t *global_evbase_handle;
extern int global_shutdown_in_progress;

static void s3_handle_glog_fatal_failure() { exit(1); }

void s3_terminate_sig_handler(int signum) {
  // When a daemon has been told to shutdown, there is a possibility of OS
  // sending SIGTERM when s3server runs as service hence ignore subsequent
  // SIGTERM signal.
  struct sigaction sigterm_action = {0};
  sigterm_action.sa_handler = SIG_IGN;
  sigterm_action.sa_flags = 0;
  sigaction(SIGTERM, &sigterm_action, NULL);
  global_shutdown_in_progress = 1;
  // glog does dynamic allocation, its not safe to call many functions within
  // signal handler
  // https://stackoverflow.com/questions/40049751/malloc-inside-linux-signal-handler-cause-deadlock

  S3Option *option_instance = S3Option::get_instance();
  int grace_period_sec = option_instance->get_s3_grace_period_sec();
  struct timeval loopexit_timeout = {.tv_sec = 0, .tv_usec = 0};

  // trigger rollbacks & stop handling new requests
  option_instance->set_is_s3_shutting_down(true);

  if (grace_period_sec > 5) {
    loopexit_timeout.tv_sec = grace_period_sec - 5;
  }
  // event_base_loopexit() will let event loop serve all events as usual
  // till loopexit_timeout (2 sec). After the timeout, all active events will be
  // served and then the event loop breaks.
  event_base_loopexit(global_evbase_handle, &loopexit_timeout);
  return;
}

/*
 *  <IEM_INLINE_DOCUMENTATION>
 *    <event_code>047005001</event_code>
 *    <application>S3 Server</application>
 *    <submodule>General</submodule>
 *    <description>Fatal handler triggered</description>
 *    <audience>Service</audience>
 *    <details>
 *      Fatal handler has triggered. This will cause S3 Server to crash.
 *      The data section of the event has following keys:
 *        time - timestamp.
 *        node - node name.
 *        pid  - process-id of s3server instance, useful to identify logfile.
 *        file - source code filename.
 *        line - line number within file where error occurred.
 *        signal_number - signal which invoked the handler.
 *    </details>
 *    <service_actions>
 *      Save the S3 server log files. Restart S3 server and contact development
 *      team for further investigation.
 *    </service_actions>
 *  </IEM_INLINE_DOCUMENTATION>
 */

void s3_terminate_fatal_handler(int signum) {
  // https://stackoverflow.com/questions/40049751/malloc-inside-linux-signal-handler-cause-deadlock
  // https://github.com/google/glog/releases (see google-glog 0.3.4)
  // ie reduce dynamic allocation from 3 to 1 per log message

  if (S3Option::get_instance()->do_redirection() == 0) {
    void *trace[S3_BACKTRACE_DEPTH_MAX];
    // https://stackoverflow.com/questions/46863569/receiving-signal-during-malloc
    int rc = backtrace(trace, S3_ARRAY_SIZE(trace));
    backtrace_symbols_fd(trace, rc, STDERR_FILENO);
  }
  s3_syslog(LOG_ALERT,
            "IEC:AS" S3_IEM_FATAL_HANDLER ":" S3_IEM_FATAL_HANDLER_STR);

  S3Daemonize s3daemon;
  s3daemon.delete_pidfile();
  if (!global_shutdown_in_progress) {
    // dafault handler for core dumping
    raise(signum);
  }
}

S3Daemonize::S3Daemonize() : noclose(0) {
  option_instance = S3Option::get_instance();
  if (option_instance->do_redirection() == 0) {
    noclose = 1;
  }
  pidfilename = option_instance->get_s3_pidfile();
}

void S3Daemonize::daemonize() {
  int rc;
  std::string daemon_wd;

  struct sigaction s3hup_act;
  memset(&s3hup_act, 0, sizeof s3hup_act);
  s3hup_act.sa_handler = SIG_IGN;

  rc = daemon(1, noclose);
  if (rc) {
    s3_log(S3_LOG_FATAL, "", "Failed to daemonize s3 server, errno = %d\n",
           errno);
    exit(1);
  }
  sigaction(SIGHUP, &s3hup_act, NULL);

  // Set the working directory for current instance as s3server-process_fid
  std::string process_fid = option_instance->get_clovis_process_fid();
  // Remove the surrounding angle brackets <>
  process_fid.erase(0, 1);
  process_fid.erase(process_fid.size() - 1);
  google::InstallFailureFunction(&s3_handle_glog_fatal_failure);
  daemon_wd = option_instance->get_daemon_dir() + "/s3server-" + process_fid;
  if (access(daemon_wd.c_str(), F_OK) != 0) {
    s3_log(S3_LOG_FATAL, "", "The directory %s doesn't exist, errno = %d\n",
           daemon_wd.c_str(), errno);
  }

  if (::chdir(daemon_wd.c_str())) {
    s3_log(S3_LOG_FATAL, "", "Failed to chdir to %s, errno = %d\n",
           daemon_wd.c_str(), errno);
  }
  s3_log(S3_LOG_INFO, "", "Working directory for S3 server = [%s]\n",
         daemon_wd.c_str());
  write_to_pidfile();
}

int S3Daemonize::write_to_pidfile() {
  std::string pidstr;
  std::ofstream pidfile;
  pidstr = std::to_string(getpid());
  pidfile.open(pidfilename);
  if (pidfile.fail()) {
    s3_log(S3_LOG_ERROR, "", "Failed to open pid file %s\n errno = %d",
           pidfilename.c_str(), errno);
    goto FAIL;
  }
  if (!(pidfile << pidstr)) {
    s3_log(S3_LOG_ERROR, "", "Failed to write to pid file %s errno = %d\n",
           pidfilename.c_str(), errno);
    goto FAIL;
  }
  pidfile.close();
  return 0;
FAIL:
  return -1;
}

int S3Daemonize::delete_pidfile() {
  char pidstr_read[100];
  int rc;
  std::ifstream pidfile_read;
  s3_log(S3_LOG_DEBUG, "", "Entering");
  if (pidfilename == "") {
    s3_log(S3_LOG_ERROR, "", "pid filename %s doesn't exist\n",
           pidfilename.c_str());
    s3_log(S3_LOG_DEBUG, "", "Exiting");
    return 0;
  }
  pidfile_read.open(S3Daemonize::pidfilename);
  if (pidfile_read.fail()) {
    if (errno == 2) {
      s3_log(S3_LOG_DEBUG, "", "Exiting");
      return 0;
    } else {
      s3_log(S3_LOG_ERROR, "", "Failed to open pid file %s errno = %d\n",
             pidfilename.c_str(), errno);
    }
    s3_log(S3_LOG_DEBUG, "", "Exiting");
    return -1;
  }
  pidfile_read.getline(pidstr_read, 100);
  if (strlen(pidstr_read) == 0) {
    s3_log(S3_LOG_ERROR, "", "Pid doesn't exist within %s\n",
           pidfilename.c_str());
    s3_log(S3_LOG_DEBUG, "", "Exiting");
    return -1;
  }
  pidfile_read.close();
  if (pidstr_read != std::to_string(getpid())) {
    s3_log(
        S3_LOG_WARN, "-",
        "The pid(%d) of process does match to the pid(%s) in the pid file %s\n",
        getpid(), pidstr_read, pidfilename.c_str());
    s3_log(S3_LOG_DEBUG, "", "Exiting");
    return -1;
  }
  rc = ::unlink(pidfilename.c_str());
  if (rc) {
    s3_log(S3_LOG_WARN, "", "File %s deletion failed\n", pidfilename.c_str());
    s3_log(S3_LOG_DEBUG, "", "Exiting");
    return rc;
  }
  s3_log(S3_LOG_DEBUG, "", "Exiting");
  return 0;
}

void S3Daemonize::register_signals() {
  struct sigaction s3action;
  memset(&s3action, 0, sizeof s3action);

  s3action.sa_handler = s3_terminate_sig_handler;
  sigaction(SIGTERM, &s3action, NULL);
  sigaction(SIGINT, &s3action, NULL);

  struct sigaction fatal_action;
  memset(&fatal_action, 0, sizeof fatal_action);

  fatal_action.sa_handler = s3_terminate_fatal_handler;
  // Call default signal handler if at all heap corruption creates another
  // SIGSEGV within signal handler
  fatal_action.sa_flags = SA_RESETHAND | SA_NODEFER;

  sigaction(SIGSEGV, &fatal_action, NULL);
  sigaction(SIGABRT, &fatal_action, NULL);
  sigaction(SIGILL, &fatal_action, NULL);
  sigaction(SIGFPE, &fatal_action, NULL);
  sigaction(SIGSYS, &fatal_action, NULL);
  sigaction(SIGXFSZ, &fatal_action, NULL);
  sigaction(SIGXCPU, &fatal_action, NULL);
}

int S3Daemonize::get_s3daemon_redirection() { return noclose; }
