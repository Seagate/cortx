#  Build Installation - FAQs

     Qs1. In cortx-s3 during init.sh installation an error occurs: 
          http://ssc-satellite1.colo.seagate.com/pulp/repos/CORTX/Production/CentOS-7_8_2003/custom/EPEL-7/EPEL-7/repodata/repomd.xml: 
          [Errno 14] curl#6 - "Could not resolve host: ssc-satellite1.colo.seagate.com; Unknown error".
  
     Ans1. Use the following steps:                   
           1. Step1: Navigate to the cloned repository 
           
           * `$ cd /root/cortx-s3server/ansible/files/yum.repos.d/centos7.8.2003`
           
           2. Step2:  
           
           * `$ [root@localhost centos7.8.2003]# ls -lrt`
             total 12
             -rw-r--r-- 1 root root 178 Jul  8 21:25 epel7.repo
             -rw-r--r-- 1 root root 216 Jul  8 21:30 cortx_s3_deps.repo
             -rw-r--r-- 1 root root 206 Jul  8 21:30 cortx.repo
           
           3. Step3: Comment all the lines in the repository listed in Step2.
           * `$ [root@localhost centos7.8.2003]# cat *.repo`
             #[releases_cortx]
             #baseurl = http://ci-storage.mero.colo.seagate.com/releases/cortx/github/main/rhel-7.8.2003/last_successful/
             #gpgcheck = 0
             #name = Yum repo for cortx builds - OS Centos 7.8.2003
             #priority = 1
             #[releases_cortx_s3deps]
             #baseurl = http://ci-storage.mero.colo.seagate.com/releases/cortx/s3server_uploads/centos7/
             #gpgcheck = 0
             #name = Yum repo for s3 dependencies rpms built for cortx - OS Centos/rhel 7
             #priority = 1
             #[epel7]
             #baseurl = http://ssc-satellite1.colo.seagate.com/pulp/repos/CORTX/Production/CentOS-7_8_2003/custom/EPEL-7/EPEL-7/
             #gpgcheck = 0
             #name = Yum repo for epel7
             #priority = 1


     Qs2:  lctl list_nids command does not show any output.
     
     Ans2: Check following entry:
           1. Step1: 
           * `$ cat /etc/modprobe.d/lnet.conf`
             options lnet networks=tcp(ens160) config_on_load=1
             (Please ensure that the above command o/p contains this entry)
           
           2. Step2: If any changes made in Step1 then:
           * `$ systemctl restart lnet`
           * `$ systemctl status lnet`
           (status should show lnet in running state)
           
           3. Step3: 
           * `$ lctl list_nids` 
            (O/p should be Eg: <ip address of VM>@tcp )
            
     Qs3: rmmod: ERROR: Module m0tr is in use or rmmod: ERROR: Module galois is in use by: m0tr or 
          insmod: ERROR: could not insert module /root/cortx-motr/extra-libs/galois/src/linux_kernel/galois.ko: File exists Error unloading /root/cortx-motr/m0tr.ko.
          
     Ans3: This happens because previous runs may have exited abruptly or because the runs was forcefully stopped using ctrl+c.
           1. Step1: List the modules.
           * `$ [root@localhost ~]# lsmod`
             Module                  Size  Used by
             m0ctl                  48916  0
             m0tr                13901353  1 m0ctl
             galois                 22944  1 m0tr
             ksocklnd              183608  0
             lnet                  586401  3 m0tr,ksocklnd
           (Note: lsmode o/p displays list of modules along with m0ctl, m0tr and galios.)
           
           2. Step2:  Remove the modules in order.
           * `$ [root@localhost ~]# rmmod m0ctl`
           * `$ [root@localhost ~]# rmmod m0tr`
           * `$ [root@localhost ~]# rmmod galois`
          
           You might couldn't see which module is using m0tr, in this case, reboot should solve the problem.
           
     Qs4: Error: no package found for log4cxx_cortx.
     
     Ans4: This issue is resolved in the section 'Create a local repository' in https://github.com/Seagate/cortx/blob/main/CONTRIBUTING.md). 

