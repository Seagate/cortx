ARG  CENTOS_RELEASE=7
FROM centos:${CENTOS_RELEASE}

# ARG vars declared before FROM aren't visible in the rest of the file unless
# they're re-declared
ARG CENTOS_RELEASE

# disable all repos except for the specific release we're targeting
RUN yum-config-manager --disable '*'
COPY C${CENTOS_RELEASE}.repo /etc/yum.repos.d/

# upgrade all
RUN yum --assumeyes upgrade

# enable EPEL and SCL repos
RUN yum --assumeyes install \
        centos-release-scl \
        epel-release

# workaround broken CentOS 7.6 compatibility witn EPEL
RUN if (( $(grep -Po '(?<=release 7\.).' /etc/redhat-release) < 7 )) ; then \
        yum install -y --disablerepo='*' --enablerepo=base --enablerepo=updates \
            python3{,-{devel,pip,setuptools}} ; \
    fi

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
