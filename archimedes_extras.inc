<?php

/**
 * @file
 */

/**
 * Find Git repos in the site root, report if there are unstaged /
 * uncommitted changes.
 */
function archimedes_extras_git_status() {
  $values = array();
  // Some tests. We ignore filemode changes where relevant.
  $tests = array(
    // Files not yet part of the repo.
    'new' => 'git ls-files --others --exclude-standard',
    // Files modified but not yet staged for commit.
    'unstaged' => 'git -c core.fileMode=false diff-files --name-status -r --ignore-submodules',
    // Files staged for commit but not committed.
    'uncommitted' => 'git -c core.fileMode=false diff-index --cached --name-status -r --ignore-submodules HEAD',
  );

  $prev_path = getcwd();
  $command = 'find . -name ".git" -type d';

  // Only show top level repo unless configured to show others.
  if (!variable_get('archimedes_extras_git_subrepos', FALSE)) {
    $command .= ' -maxdepth 1';
  }

  exec("$command 2>/dev/null", $output);
  foreach ($output as $path) {
    $found[] = realpath(dirname($path));
  }
  sort($found);
  foreach ($found as $directory) {
    $output = array();
    chdir($directory);
    foreach ($tests as $label => $test) {
      $output = array();
      exec($test, $output);
      if (!empty($output)) {
      /*
        $owners = array();
        foreach ($output as $file) {
          $owner = posix_getpwuid(fileowner($file));
          if (!in_array($owner, $owners)) {
            $owners[] = $owner;
          }
        }
      */
        $message = '<strong>!type files in !directory</strong>';
        $result[$directory][$label]['header'] = t($message, array(
                                                    '!directory' => $directory,
                                                    '!type' => $label,
                                                    // '%owners' => implode(', ', $owners),
                                                  ));
        $result[$directory][$label]['body'] = $output;
      }
    }
  }
  chdir($prev_path);
  if (!empty($result)) {
    foreach ($result as $directory => $reports) {
      foreach ($reports as $label => $report) {
        $values[] = $report['header'] . '<pre>' . implode("\n", $report['body']) . '</pre>';
      }
    }
  }
  return $values;
}

/**
 * Gets CiviCRM Git hash.
 */
function archimedes_extras_civicrm_gitinfo() {
  if (module_exists('civicrm')) {
    civicrm_initialize();
    global $civicrm_root;
    if (file_exists($civicrm_root . '/.git')) {
      $prev_path = getcwd();
      chdir($civicrm_root);

      $cmds = array(
        'commit_hash' => 'git rev-parse --short HEAD',
        'branch' => 'git rev-parse --abbrev-ref HEAD',
      );
      foreach ($cmds as $key => $cmd) {
        $output = array();
        // Get current commit hash.
        exec($cmd, $output);
        if (!empty($output)) {
          $info[$key] = reset($output);
        }
      }

      chdir($prev_path);
    }
  }
  if (!empty($info)) {
    return $info;
  }
}

/**
 * Report time since previous CiviCRM cron run.
 */
function archimedes_extras_civicrm_lastcron() {
  if (module_exists('civicrm')) {
    civicrm_initialize();

    try {
      $result = civicrm_api3('JobLog', 'get', array(
        // 'name' => 'Send Scheduled Mailings'
        'sequential' => 1,
        'options' => array(
          'limit' => 1,
          'sort' => "run_time desc"
        ),
      ));

      if (isset($result['values'][0]['run_time'])) {
        // civicrm_job_log does not have TZ info, so get from site (?).
        $tz = variable_get('date_default_timezone', 'UTC');
        // Feels crusty.
        $timestr = $result['values'][0]['run_time'] . ' ' . $tz;
        if ($run_time = strtotime($timestr)) {
          $since = time() - $run_time;
        }
      }
      else {
        $since = -1;
      }

      if ($since > 3600 || $since === -1) {
        $when = ($since == -1) ? 'never' : date('c', $lastCron);
        watchdog('archimedes_extras', 'CiviCRM cron may not be running? Last execution was @time.', array('@time' => $when), WATCHDOG_WARNING);
      }
    }
    catch (Exception $e) {
      watchdog('archimedes_extras', 'Unable to get job info from CiviCRM: %message', array('%message' => $e->getMessage()), WATCHDOG_ERROR);
    }

    return $since;
  }
}