# Single Node K8s Cortx Deployment

The following steps are presented to deploy Cortx deployment on a single node K8s cluster. The control node acts as both master and worker as it's untainted.

# Prerequisites

The following requirements must exists for deployment:
- 1 VM with CentOS 7.9.2009 OS.
- CPU: 8 cores
- RAM: 8 GB
- Minimum additional disks: 8 disks (6 Disks: Cortx Software, 1 Disk: 3rd Party Software, 1 Disk: Log)
> Example 8 drives (the actual device names might be different):
```
/dev/sdb => 25G
/dev/sdc => 25G                     
/dev/sdd => 25G                     
/dev/sde => 25G                      
/dev/sdf => 25G                      
/dev/sdg => 25G                      
/dev/sdh => 25G   
```

# Installation

The following primary packages version are required:
- Kubernetes Version - 1.19.0-0
- Calico Plugin Version - Latest ( v3.20.1 )
- Docker version - Latest community edition ( 20.10.8 )

**Additional packages:**
- Helm - Latest version (3.7.1)
- yq

All the steps are run using ```root``` user and firewall is stopped and selinux is in permissive mode as follows:
Firewall stop/disable:
```
systemctl stop firewalld
systemctl disable firewalld
```
SELinux permissive mode:
```
sudo setenforce 0
sudo sed -i 's/^SELINUX=enforcing$/SELINUX=permissive/' /etc/selinux/config
``` 

**Docker installation:**
To install docker run the following  commands:
```
curl -fsSL https://get.docker.com -o get-docker.sh
chmod +x get-docker.sh
```
**Optional:** Docker Hub rate limits
Docker hub sets pull rate limits for anonymous requests to 100 container image pull requests per six hours which may cause issues with the installation. You may need to create an account on Docker Hub to increase rate limit to free user to 200 container image pull requests per six hours and pull required public images on all worker nodes. Use the following command to do so (replace username and password):
```
docker login --username=<Docker username> --password <Docker password>
```

**Kubernetes installation:**
To install Kubernetes run the following commands:
```
cat <<EOF | sudo tee /etc/modules-load.d/k8s.conf
br_netfilter
EOF

cat <<EOF | sudo tee /etc/sysctl.d/k8s.conf
net.bridge.bridge-nf-call-ip6tables = 1
net.bridge.bridge-nf-call-iptables = 1
EOF
sudo sysctl --system
```
Add Kubernetes repository:
```
cat <<EOF | sudo tee /etc/yum.repos.d/kubernetes.repo
[kubernetes]
name=Kubernetes
baseurl=https://packages.cloud.google.com/yum/repos/kubernetes-el7-\$basearch
enabled=1
gpgcheck=1
repo_gpgcheck=1
gpgkey=https://packages.cloud.google.com/yum/doc/yum-key.gpg https://packages.cloud.google.com/yum/doc/rpm-package-key.gpg
exclude=kubelet kubeadm kubectl
EOF
```
Install Kubernetes:
```
yum install -y kubectl-1.19.0-0 kubelet-1.19.0-0 kubeadm-1.19.0-0 --disableexcludes=kubernetes
sudo systemctl enable --now kubelet
```
