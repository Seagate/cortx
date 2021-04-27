# How to integrate CORTX with Google Compute Engine and Strapi

<!-- <center><img src="https://miro.medium.com/max/650/1*46UfaJiAcTnlC85L40T-Nw.png"/></center>
<center><img src="https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://snipcart.com/media/204940/strapi-tutorial.png"/></center>
<center><img src="https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://www.seagate.com/www-content/product-content/storage/object-storage-software/_shared/images/enterprise-cortex-pdp-row1-content-image.png"/></center> -->

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

<!-- [![IMAGE ALT TEXT HERE](https://img.youtube.com/vi/7VIXAd1bEDk/0.jpg)](https://www.youtube.com/watch?v=7VIXAd1bEDk) -->

# Install cortx-s3server in Google Compute Engine

So in order for our cortx-s3server to works, first we need to create an image spesifically for google compute engine, because cortx-s3server can't be build in other image other than centos7.8.2003 up until now, and should be using kernel 3.10.0-1127, so first of all open your virtualbox and create a virtual machine :

![create virtual machine](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/1858b0438cb92d17be555dc641ebadcc.png)

You can named it whatever you want, the most important part is you should choose type `linux` and choose version `other linux (64-bit)` then click next

![ram](https://linkedin-resume-spiritbro.vercel.app/api/convertimage?url=https://i.gyazo.com/d78fb909dff8ae0d939f9fa5b62fd2c8.png)

# Install strapi

# Error you may encounter

# References

- [strapi s3](https://dev.to/kevinadhiguna/how-to-setup-amazon-s3-upload-provider-in-your-strapi-app-1opc)
