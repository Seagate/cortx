set -euE
export LOG_FILE="${LOG_FILE:-/var/log/seagate/provisioner/va_bootstrap.log}"
mkdir -p $(dirname "${LOG_FILE}")

function trap_handler {
    echo "***** ERROR! *****"
    echo "For detailed error logs, please see: $LOG_FILE"
    echo "******************"
}
trap trap_handler ERR
BASEDIR=$(dirname "${BASH_SOURCE}")

mgmt_ip=$(ip -4 addr show ens32 | grep -oP '(?<=inet\s)\d+(\.\d+){3}')
public_ip=$(ip -4 addr show ens33 | grep -oP '(?<=inet\s)\d+(\.\d+){3}')
private_ip=$(ip -4 addr show ens34 | grep -oP '(?<=inet\s)\d+(\.\d+){3}')

sed -i "s/MANAGEMENT_IP=\b\([0-9]\{1,3\}\.\)\{1,3\}[0-9]\{1,3\}/MANAGEMENT_IP=$mgmt_ip/g" /opt/seagate/cortx/csm/web/web-dist/.env
sed -i "s/\b\([0-9]\{1,3\}\.\)\{1,3\}[0-9]\{1,3\}\s*srvnode-1.mgmt.public/$mgmt_ip\tsrvnode-1.mgmt.public/g" /etc/hosts
sed -i "s/\b\([0-9]\{1,3\}\.\)\{1,3\}[0-9]\{1,3\}\s*srvnode-1.data.public/$public_ip\tsrvnode-1.data.public/g" /etc/hosts
echo $private_ip srvnode-1 srvnode-1.data.private | sudo tee -a /etc/hosts
sed -i 's/BIND=192.168.220.120/BIND='$private_ip'/g' /var/lib/hare/consul-env
sed -i 's/CLIENT=127.0.0.1 192.168.220.120/CLIENT=127.0.0.1 '$private_ip'/g' /var/lib/hare/consul-env

# Reconfigure Pillar
provisioner configure_setup ~/config.ini 1
provisioner confstore_export

# Reconfigure Network
salt-call state.apply components.system.network
salt-call state.apply components.system.network.data.public
salt-call state.apply components.system.network.data.direct
salt-call state.apply components.system.config.sync_salt

# Reconfigure /etc/hosts
salt-call state.apply components.system.config.hosts

# Reconfigure firewall
salt-call state.apply components.system.firewall.teardown
salt-call state.apply components.system.firewall.config

# Firewall Salt states correct all deviations on the system
# OVA environment needs corrections to firewall rules
firewall-cmd --zone=management-zone --add-port=80/tcp --permanent
firewall-cmd --zone=management-zone --add-port=443/tcp --permanent
firewall-cmd --zone=public-data-zone --add-port=80/tcp --permanent
systemctl restart firewalld

# Reconfigure kafka
salt-call state.apply components.misc_pkgs.kafka.config
salt-call state.apply components.misc_pkgs.kafka.start
salt-call state.apply components.cortx_utils.config

# Reconfigure lustre
salt-call state.apply components.misc_pkgs.lustre.stop
salt-call state.apply components.misc_pkgs.lustre.config

# echo "INFO: Restarting elasticsearch" | tee -a "${LOG_FILE}"
echo "INFO: Restarting elasticsearch" | tee -a "${LOG_FILE}"
systemctl restart elasticsearch

# Configure s3server
echo "INFO: Configuring haproxy and s3" | tee -a "${LOG_FILE}"
salt "*" state.apply components.s3server.config | tee -a "${LOG_FILE}"
salt "*" state.apply components.s3server.start | tee -a "${LOG_FILE}"

# Reconfigure Hare CDF
/opt/seagate/cortx/hare/bin/hare_setup init --config json:///opt/seagate/cortx_configs/provisioner_cluster.json
salt-call state.apply components.hare.config

# Configure and start sspl
echo "INFO: Configuring sspl" | tee -a "${LOG_FILE}"
salt "*" state.apply components.sspl.config | tee -a "${LOG_FILE}"
salt "*" state.apply components.sspl.start | tee -a "${LOG_FILE}"

# Restart component services
systemctl restart hare-consul-agent
systemctl restart csm_web
systemctl restart csm_agent
systemctl restart kafka
systemctl restart haproxy

# Start the CORTX Cluster
hctl start

# Create the admin user, cortxadmin
/opt/seagate/cortx/csm/bin/csm_setup config --config json:///opt/seagate/cortx_configs/provisioner_cluster.json
