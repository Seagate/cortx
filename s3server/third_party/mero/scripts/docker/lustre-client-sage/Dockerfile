ARG  CENTOS_RELEASE=sage
FROM registry.gitlab.mero.colo.seagate.com/centos:${CENTOS_RELEASE}

ARG LUSTRE_VERSION=2.12.3

COPY kmod-lustre-client-${LUSTRE_VERSION}*.rpm ./
COPY lustre-client-${LUSTRE_VERSION}*.rpm ./
COPY lustre-client-devel-${LUSTRE_VERSION}*.rpm ./

RUN yum -y install {kmod-,}lustre-client-*.rpm
