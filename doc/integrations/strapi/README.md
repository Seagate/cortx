# How to integrate CORTX with Google Compute Engine and Strapi

<center><img src="https://miro.medium.com/max/650/1*46UfaJiAcTnlC85L40T-Nw.png"/></center>
<center><img src="https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://snipcart.com/media/204940/strapi-tutorial.png"/></center>
<center><img src="https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://www.seagate.com/www-content/product-content/storage/object-storage-software/_shared/images/enterprise-cortex-pdp-row1-content-image.png"/></center>

# List of Contents
- [prerequisites](#prerequisites)
- [what is strapi](#what-is-strapi)
- [video demo](#video-demo)
- [install cortx-s3server](#install-cortx-s3server-in-google-compute-engine)
- [install strapi](#install-strapi)
- [error you may encounter](#error-you-may-encounter)
- [references](#references)

# Prerequisites

- [google cloud platform](https://cloud.google.com/)
- [virtualbox](https://www.virtualbox.org/) 
- [centos7.8.2003 minimal iso](http://mirrors.oit.uci.edu/centos/7.8.2003/isos/x86_64/)

# What is strapi

Strapi is a open source headless cms that you can use to create a beautiful website without the need of a backend service, using strapi you can create database, store image, and create api out of that more easily, however when we first starting strapi the image storage is in the public folder inside of the strapi itself, this will make our image prone to error, like when we migrate our strapi we need to migrate our public folder as well, luckily strapi have some system that can integrate with s3 easily now i'm gonna show you how to do that.

# Video demo

[![IMAGE ALT TEXT HERE](https://img.youtube.com/vi/AhFE3gelPRo/0.jpg)](https://www.youtube.com/watch?v=AhFE3gelPRo)

# Install cortx-s3server in Google Compute Engine

So in order for our cortx-s3server to works, first we need to create an image spesifically for google compute engine, because cortx-s3server can't be build in other image other than centos7.8.2003 up until now, and should be using kernel 3.10.0-1127, so first of all open your virtualbox and create a virtual machine :

![create virtual machine](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/1858b0438cb92d17be555dc641ebadcc.png)

You can named it whatever you want, the most important part is you should choose type `linux` and choose version `other linux (64-bit)` then click next

![ram](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/d78fb909dff8ae0d939f9fa5b62fd2c8.png)

Put your memory size to 2048 MB, because centos will not be created if you put default memory, then click next



![create virtual](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/c376c6f56b2ef75d99f3bab47814f292.png)

Click `create virtual hard disk now` then click `create`

![choose vmdk](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/2025b01f02b7f7359e2b98b5ff7569c5.png)

Choose VMDK then click on next

![dinamically allocated](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/2196d4866c8b7841bb63286c4668eec7.png)

Click on dinamically allocated

![storage](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/2ff8f4f04922f72d8b015345df8b8d58.png)

Set your storage to be 30 GB, because approximately when we install our cortx-s3server later it will use 15 GB of our disk so just to make it safe we put 30 GB, but basically you can resize it later as well after that click create

![start](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/4c8e16168d63b3a4a11927a4385b7616.png)

Now click on start to start it up

![choose your iso](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/acc40f13ff991abaa7376ec16614834e.png)

Choose the centos7.8.2003 minimal iso then click start

![centos](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/9b585bf3b47a2f82453a82006f941445.png)

Click install centos 7

![continue](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/8f59043149ef37afc62ccda784f96e59.png)

Click continue

![installation destination](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/9041a9213a19d5dc662b4cf3e35ac1c8.png)

Click on installation destination

![done](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/b4b85f0c6af3ca33ddd00d56deb2e849.png)

Click on ATA VBOX HARDDISK then click done

![begin](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/147fa2f30c9102df07c8f90ae8701b2f.png)

Click on begin installation

![set](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/e6e86c247522fc450429ee14bf5f4925.png)

Set the password and user as you wish and wait for the installation to complete

![finish](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/1c6bb84255d64538eda5b1dd074065c2.png)

Click on finish configuration

![reboot](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/97b016317f1b39bf43f3bcd464178784.png)

click on reboot

![login](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/975ccf760189a786d26247d04f713421.png)

After you login to centos, issue this command :

```bash
vi /etc/default/grub
```
Remove the `rhgb` and `quiet` kernel command-line arguments. and change it to `console=ttyS0,38400n8d` just like the image above then type `:wq` to close vim editor

![qemu](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/e9304c0a2f5bc30cb9b0b53a32ad854c.png)

Now go to your command line if you're on windows i recommend use wsl issue this command on terminal :

```bash
sudo apt-get install qemu -y
```

Now go to `C:\Users\<your windows username>\VirtualBox VMs\seagate` open terminal there and issue this command :

```bash
qemu-img convert -f vmdk seagate.vmdk  -O raw disk.raw
```

You will then get `disk.raw` file now let's compress this image using this command :

```bash
tar -cSzf centos782003.tar.gz disk.raw
```

Now go to your google cloud storage dashboard and upload the `centos782003.tar.gz` 

![upload](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/d48913e72640bcd0aa5dffdb4213fb22.png)

Here is the screenshot after you successfully upload it to your google cloud storage

![image](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/37823c6fe9c07e071196ae972e49d791.png)

Now go to google compute engine images dashboard and click `create image`

![image](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/549fd0edaf61bde3f04cea85b0ae7409.png)

Fill the name as you wish and select your cloud storage tar that you create earlier

Now click on `VM instances` and let's create some virtual machine by clicking `Create Instance`

![8gb](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/af68766848311958aa0029343a19e23a.png)

Pick the 8gb memory one

![change](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/a3dfc63d8d51154bc33a96d528576280.png)

Change boot disk to the one you create earlier

![allow](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/658ca9f97c0a791be8dffb7cf2125edb.png)

Allow both http and https traffic then click create

![external](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/0684e51e98e5a47901ceec0a2d43017d.png)

Copy paste your external api then open [putty](https://www.putty.org/)

![ssh](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/f9481f77a5795e180e529b2b714a2aee.png)

Copy paste the ip address to hostname and click open to login to your ssh then follow the tutorial [here](https://github.com/Seagate/cortx-s3server/blob/main/docs/CORTX-S3%20Server%20Quick%20Start%20Guide.md)

# Install strapi

Run this command:

```bash
yarn create strapi-app my-project --quickstart
```

Then stop the strapi process in terminal, then run this command :

```bash
yarn add strapi-provider-upload-aws-s3
```

Create `my-project/config/plugins.js` then fill in this code :

```javascript
module.exports = ({ env }) => ({
    upload: {
      provider: 'aws-s3',
      providerOptions: {
        s3ForcePathStyle: true,
          endpoint:"http://<your external ip address>",
          sslEnabled:false,
        accessKeyId: env('AWS_ACCESS_KEY_ID'),
        secretAccessKey: env('AWS_ACCESS_SECRET'),
        region: env('AWS_REGION'),
        
        params: {
            Bucket: env('AWS_BUCKET'),
            StorageClass: env('AWS_S3_STORAGE_CLASSES')
        },
        logger: console
      }
    }
   });
```

create `.env` in the root of the strapi project folder that we create earlier and write this code :

```
HOST=0.0.0.0
PORT=1337
AWS_ACCESS_KEY_ID=<your cortx access key id>
AWS_SECRET_ACCESS_KEY=<your cortx secret access key>
AWS_REGION=US
AWS_BUCKET=<your cortx bucket name>
AWS_S3_STORAGE_CLASSES=S3 Standard
```

Now you can go and upload some image in strapi

![strapi](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/6e2f64386b470f48785040a7096948e4.png)

Click on `Media Library`

![assets](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/d4db257c6b28a5ff3824cd734cf7adec.png)

Click on `upload assets` then upload some image then go to your putty again and run this command :

```bash
aws s3 ls s3://<your bucket name>
```
![file](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/e2e19039519e07124941c3ddfc4b0d42.png)

If you see some list like the above image then you're successfully upload from strapi to cortx-s3server in your google compute engine

# Error you may encounter

## Wrong kernel

![kernel](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/e41e1d362b3fcfe5381e90166d835847.png) 

you might find error like this to fix that run this command one by one:

```
curl https://linuxsoft.cern.ch/cern/centos/7/updates/x86_64/Packages/Packages/kernel-devel-3.10.0-1127.el7.x86_64.rpm -o kernel.rpm
rpm -i kernel.rpm
```
## Wrong url of kafka in documentation

When you try to run this command :

```
curl "http://cortx-storage.colo.seagate.com/releases/cortx/third-party-deps/centos/centos-7.8.2003-2.0.0-latest/commons/kafka/kafka-2.13_2.7.0-el7.x86_64.rpm" -o kafka.rpm
```

you will got this error :

```
Could not resolve host: cortx-storage.colo.seagate.com; Unknown error
```
To fix that run this command one by one:

```bash
mkdir kafka
cd kafka
curl https://www.apache.org/dyn/closer.cgi?path=/kafka/2.7.0/kafka_2.13-2.7.0.tgz -o kafka.tgz
tar xf kafka.tgz --strip 1
yum -y install java-1.8.0-openjdk
cd ..
ln -s kafka kafka
echo "export PATH=$PATH:/root/kafka/bin" >> ~/.bash_profile
source ~/.bash_profile
zookeeper-server-start.sh -daemon /root/kafka/config/zookeeper.properties
kafka-server-start.sh -daemon /root/kafka/config/server.properties
```



# References

- [strapi s3](https://dev.to/kevinadhiguna/how-to-setup-amazon-s3-upload-provider-in-your-strapi-app-1opc)
- https://www.tecmint.com/install-apache-kafka-in-centos-rhel/
- https://www.turnkeylinux.org/blog/convert-vdi-vmdk
- https://linuxsoft.cern.ch/cern/centos/7/updates/x86_64/repoview/kernel-devel.html