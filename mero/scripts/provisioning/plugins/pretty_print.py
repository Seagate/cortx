# Ansible plugin for pretty-printing playbook tasks output

from __future__ import (absolute_import, division, print_function)
from ansible.plugins.callback import CallbackBase
import json
__metaclass__ = type

RECORDS = (
    #'failed',
    'msg',
    'reason',
    'results',
    'stderr',
    'stdout',
)

class CallbackModule(CallbackBase):


    CALLBACK_NAME = 'pretty_print'
    CALLBACK_TYPE = 'notification'
    CALLBACK_VERSION = 2.0
    CALLBACK_NEEDS_WHITELIST = False

    #
    # helpers ------------------------------
    #

    def pretty_print(self, data):
        if type(data) is dict:
            for rec in RECORDS:
                no_log = data.get('_ansible_no_log', False)
                if rec in data and data[rec] and not no_log:
                    output = self._format(data[rec]).replace("\\n", "\n")
                    self._display.display("{0}:  {1}".format(rec, output),
                                          log_only=False)

    def _format(self, output):
        if type(output) is dict:
            return json.dumps(output, indent=2, sort_keys=True)

        # output may contain nested results when a task uses 'with_items'
        if type(output) is list and type(output[0]) is dict:
            formatted_output = []
            for i, elem in enumerate(output):
                if type(elem) is dict:
                    for rec in set(RECORDS) & set(elem):
                        formatted_output.append( self._format(elem[rec]) )
            if len(formatted_output) == 1:
                return formatted_output[0]
            else:
                return '\n  ' + '\n  '.join(formatted_output)

        if type(output) is list and type(output[0]) is not dict:
            if len(output) == 1:
                return output[0]
            else:
                return '\n  ' + '\n  '.join(output)

        return str(output)

    #
    # V1 methods ---------------------------
    #

    def runner_on_async_failed(self, host, res, jid):
        self.pretty_print(res)

    def runner_on_async_ok(self, host, res, jid):
        self.pretty_print(res)

    def runner_on_async_poll(self, host, res, jid, clock):
        self.pretty_print(res)

    def runner_on_failed(self, host, res, ignore_errors=False):
        self.pretty_print(res)

    def runner_on_ok(self, host, res):
        self.pretty_print(res)

    def runner_on_unreachable(self, host, res):
        self.pretty_print(res)

    #
    # V2 methods ---------------------------
    #

    def v2_runner_on_async_failed(self, result):
        self.pretty_print(result._result)

    def v2_runner_on_async_ok(self, host, result):
        self.pretty_print(result._result)

    def v2_runner_on_async_poll(self, result):
        self.pretty_print(result._result)

    def v2_runner_on_failed(self, result, ignore_errors=False):
        self.pretty_print(result._result)

    def v2_runner_on_ok(self, result):
        self.pretty_print(result._result)

    def v2_runner_on_unreachable(self, result):
        self.pretty_print(result._result)
