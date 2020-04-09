FROM centos:7

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
