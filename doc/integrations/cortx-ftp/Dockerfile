FROM ubuntu:20.04

RUN apt update
RUN apt upgrade

RUN apt-get install -y s3fs
RUN apt-get install -y vsftpd
RUN sed -i 's/anonymous_enable=NO/anonymous_enable=YES/' /etc/vsftpd.conf
RUN mkdir /srv/ftp/cortx-fs
RUN chmod 600 /srv/ftp/cortx-fs

ENTRYPOINT service vsftpd start && bash