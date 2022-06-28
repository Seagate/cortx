#!/bin/bash
#
# Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For any questions about this software or licensing,
# please email opensource@seagate.com or cortx-questions@seagate.com.
#

FILE_LOCATION="/opt/seagate/cortx"
echo -e "Generating RELEASE.INFO file"

#EXCLUDE_PACKAGES="cortx-motr-devel\|cortx-motr-tests-ut\|cortx-libsspl_sec-devel\|cortx-libsspl_sec-method_pki\|cortx-prvsnr-cli\|cortx-sspl-cli\|cortx-s3iamcli-devel\|cortx-sspl-test"
pushd "$FILE_LOCATION" || exit
cat <<EOF > RELEASE.INFO
---
NAME: "CORTX"
OS: $(cat /etc/redhat-release | sed -e 's/ $//g' -e 's/^/\"/g' -e 's/$/\"/g')
DATETIME: $(date +"%d-%b-%Y %H:%M %Z" | sed -e 's/^/\"/g' -e 's/$/\"/g')
COMPONENTS:
$(for component in $(sed 's/#.*//g' /opt/seagate/cortx/cortx-componenet-rpms.txt); do echo "    - \"$(rpm -q $component).rpm\"" ; done)
THIRD_PARTY_RPM_PACKAGES:
$(for component in $(sed 's/#.*//g' /opt/seagate/cortx/third-party-rpms.txt); do echo "    - \"$(rpm -q $component).rpm\"" ; done)
THIRD_PARTY_PYTHON_PACKAGES:
$(pip3 freeze | sed 's/^/    - /g')
EOF
popd || exit
