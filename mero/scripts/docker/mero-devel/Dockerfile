ARG  CENTOS_RELEASE=7
ARG  LUSTRE_VERSION=2.12.3
FROM registry.gitlab.mero.colo.seagate.com/lustre-client:${CENTOS_RELEASE}-${LUSTRE_VERSION}

COPY mero.spec .

RUN sed -i 's/@.*@/111/g' mero.spec
RUN kernel_src=$(ls -1rd /lib/modules/*/build | head -n1) \
    yum-builddep -y mero.spec

RUN rm -f mero.spec
