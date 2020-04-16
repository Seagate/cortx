ARG  CENTOS_RELEASE=7
FROM registry.gitlab.mero.colo.seagate.com/mero/mero:${CENTOS_RELEASE}

COPY s3server-uploads.repo /etc/yum.repos.d/
COPY s3rpm.spec .

RUN yum install -y yum-priorities

RUN yum install -y http://ci-storage.mero.colo.seagate.com/releases/master/mero_stub/mero-0.0.0-stub.x86_64.rpm \
                   http://ci-storage.mero.colo.seagate.com/releases/master/mero_stub/mero-devel-0.0.0-stub.x86_64.rpm \
    && yum install -y git-clang-format \
    && yum-builddep -y --define 'python3_other_pkgversion 36' s3rpm.spec \
    && rm -f s3rpm.spec \
    && yum remove -y mero mero-devel
