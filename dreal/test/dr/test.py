# -*- coding: utf-8 -*-
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import sys
import subprocess
import difflib

# 1st Argument: dreal path
#               need to check if it exists
dreal = sys.argv[1]

# 2nd Argument: dr formula name
dr = sys.argv[2]

# 3rd Argument: dr expected output
expected_output_filename = sys.argv[3]
with open(expected_output_filename, "r") as myfile:
    expected_output = myfile.read().strip().splitlines()

rest_of_args = sys.argv[4:]

try:
    # 1. Run dReal with dr file
    output = subprocess.check_output([dreal, dr] +
                                     rest_of_args).decode('UTF-8')
    output = output.splitlines()
    # 2. Compare the output with expected output
    diff_result = list(
        difflib.unified_diff(output,
                             expected_output,
                             fromfile='output',
                             tofile='expected output',
                             lineterm=''))
    if diff_result:
        # 3. They are not the same, show the diff.
        for line in diff_result:
            print(line)
        sys.exit(1)
    else:
        # 4. They are the same.
        sys.exit(0)

except subprocess.CalledProcessError as grepexc:
    print("error code", grepexc.returncode, grepexc.output)
    sys.exit(grepexc.returncode)
