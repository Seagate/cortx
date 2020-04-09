#
# Builder image
#

ARG  CENTOS_RELEASE=7
FROM registry.gitlab.mero.colo.seagate.com/centos:${CENTOS_RELEASE} as lustre-rpm-builder

ARG LUSTRE_VERSION=2.12.3

COPY lustre-2.9.0.spec.patch .
ADD  https://downloads.whamcloud.com/public/lustre/lustre-${LUSTRE_VERSION}/el7/client/SRPMS/lustre-${LUSTRE_VERSION}-1.src.rpm .

RUN rpm -i lustre-${LUSTRE_VERSION}-1.src.rpm
RUN patch -p1 -i lustre-2.9.0.spec.patch rpmbuild/SPECS/lustre.spec
RUN yum -y install kernel gcc make
RUN yum-builddep -y rpmbuild/SPECS/lustre.spec
RUN QA_RPATHS=255 rpmbuild -bb --without servers --without lustre-tests \
        --define 'configure_args --disable-gss-keyring' \
        --define "kver `ls -1r /lib/modules | head -n1`" \
        rpmbuild/SPECS/lustre.spec

#
# Final image
#

ARG  CENTOS_RELEASE=7
FROM registry.gitlab.mero.colo.seagate.com/centos:${CENTOS_RELEASE}

ARG LUSTRE_VERSION=2.12.3

COPY --from=lustre-rpm-builder /root/rpmbuild/RPMS/x86_64/lustre-client-devel-${LUSTRE_VERSION}*.rpm .

RUN wget https://downloads.whamcloud.com/public/lustre/lustre-${LUSTRE_VERSION}/el7/client/RPMS/x86_64/kmod-lustre-client-${LUSTRE_VERSION}-1.el7.x86_64.rpm \
         https://downloads.whamcloud.com/public/lustre/lustre-${LUSTRE_VERSION}/el7/client/RPMS/x86_64/lustre-client-${LUSTRE_VERSION}-1.el7.x86_64.rpm

RUN yum --assumeyes install \
        kmod-lustre-client-${LUSTRE_VERSION}*.rpm \
        lustre-client-${LUSTRE_VERSION}*.rpm \
        lustre-client-devel-${LUSTRE_VERSION}*.rpm
