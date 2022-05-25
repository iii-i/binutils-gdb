# Copyright (C) 2022 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import os
import shlex

from perftest import measure
from perftest import perftest
from perftest import testresult
from perftest import utils


class JitCheck(perftest.TestCaseWithBasicMeasurements):
    def __init__(self, name, run_names, binfile):
        super(JitCheck, self).__init__(name)
        self.run_names = run_names
        self.binfile = binfile
        self.measurement = measure.MeasurementWallTime(
            testresult.SingleStatisticTestResult())
        self.measure.measurements.append(self.measurement)

    def warm_up(self):
        pass

    def execute_test(self):
        for run in self.run_names:
            exe = "{}-{}".format(self.binfile, utils.convert_spaces(run))
            utils.select_file(exe)
            pieces = os.path.join(os.path.dirname(exe), "pieces")
            gdb.execute("cd {}".format(shlex.quote(pieces)))
            utils.runto_main()
            gdb.execute("tbreak done_breakpoint")
            self.measure.measure(lambda: gdb.execute("continue"), run)
            for array_name in ("register_times", "unregister_times"):
                array_value = gdb.parse_and_eval(array_name)
                for i in range(*array_value.type.range()):
                    parameter = "{}:{}:{}".format(run, array_name, i + 1)
                    result = int(array_value[i]) / 1E+6
                    self.measurement.result.record(parameter, result)
