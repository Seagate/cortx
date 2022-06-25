# Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
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
variable "security_group_cidr" {
  description = "Value of CIDR block to be used for Security Group. This should be your systems public-ip"
  type        = string
}


variable "os_version" {
  description = "OS Version"
  type        = string
}


variable "region" {
  description = "Region"
  type        = string
}

variable "key_name" {
  description = "SSH Key name"
  type        = string
  default     = "cortx-key"
}