# TODO: use fixed 7.7.XXXX tag when 7.7 is frozen
ARG  CENTOS_RELEASE_SAGE=7
FROM centos:${CENTOS_RELEASE_SAGE}

# ARG vars declared before FROM aren't visible in the rest of the file unless
# they're re-declared
ARG CENTOS_RELEASE_SAGE

# disable all repos except for the specific release we're targeting
#RUN yum-config-manager --disable '*'
#COPY C${CENTOS_RELEASE_SAGE}.repo /etc/yum.repos.d/

# upgrade all
RUN yum --assumeyes upgrade

# match SAGE kernel
RUN yum -y remove 'kernel*'
RUN yum -y install kernel-3.10.0-1062.4.1.el7

# enable EPEL and SCL repos
RUN yum --assumeyes install \
        centos-release-scl \
        epel-release

# install common tools
RUN yum --assumeyes install \
        createrepo \
        file \
        git \
        jq \
        rpm-build \
        rpmdevtools \
        scl-utils \
        sclo-git212 \
        wget \
        which

# mark special release
RUN sed -i -e 's/Core/SAGE/' /etc/redhat-release

# enable Git from SLC repo (it's more up to date version)
RUN ln -nsf /opt/rh/sclo-git212/root/bin/* /usr/local/bin/ \
    && ln -nsf /opt/rh/sclo-git212/enable /etc/profile.d/sclo-git212.sh

# set locale
RUN localedef -i en_US -f UTF-8 en_US.UTF-8
ENV LANG=en_US.UTF-8 \
    LC_ALL=en_US.UTF-8

# set up rpmbuild directory
RUN rpmdev-setuptree

WORKDIR /root
