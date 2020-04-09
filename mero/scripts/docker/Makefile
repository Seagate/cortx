.PHONY: latest 7
latest 7: centos-image lustre-client-image mero-devel-image
	$(DOCKER) tag $(NAMESPACE)/mero-devel:$(CENTOS_RELEASE) \
				  $(NAMESPACE)/mero/mero:$(basename $(CENTOS_RELEASE))

CENTOS_RELEASE  := 7
LUSTRE_VERSION  := 2.12.3
NAMESPACE       := registry.gitlab.mero.colo.seagate.com
DOCKER          := docker


.PHONY: 7.6 7.5 7.3 \
        centos-image \
        centos-vault-image \
        centos-sage-image \
        clean \
        eos \
        lustre-client-image \
        lustre-client-sage-image \
        mero-devel-image \
        push \
        sage

7.6: CENTOS_RELEASE := 7.6.1810
7.6: LUSTRE_VERSION := 2.10.8
7.6: centos-vault-image lustre-client-image mero-devel-image
	$(DOCKER) tag $(NAMESPACE)/mero-devel:$(CENTOS_RELEASE) \
				  $(NAMESPACE)/mero/mero:$(basename $(CENTOS_RELEASE))

7.5: CENTOS_RELEASE := 7.5.1804
7.5: LUSTRE_VERSION := 2.10.5
7.5: centos-vault-image lustre-client-image mero-devel-image
	$(DOCKER) tag $(NAMESPACE)/mero-devel:$(CENTOS_RELEASE) \
				  $(NAMESPACE)/mero/mero:$(basename $(CENTOS_RELEASE))

7.3: CENTOS_RELEASE := 7.3.1611
7.3: LUSTRE_VERSION := 2.9.0
7.3: centos-vault-image lustre-client-image mero-devel-image
	$(DOCKER) tag $(NAMESPACE)/mero-devel:$(CENTOS_RELEASE) \
				  $(NAMESPACE)/mero/mero:$(basename $(CENTOS_RELEASE))
	$(DOCKER) tag $(NAMESPACE)/mero-devel:$(CENTOS_RELEASE) \
				  $(NAMESPACE)/mero/mero:sage-vm

sage: CENTOS_RELEASE := sage
sage: LUSTRE_VERSION := 2.12.3
sage: centos-sage-image lustre-client-image mero-devel-image
	$(DOCKER) tag $(NAMESPACE)/mero-devel:$(CENTOS_RELEASE) \
				  $(NAMESPACE)/mero/mero:$(basename $(CENTOS_RELEASE))

eos: centos-image lustre-client-image mero-devel-image
	$(DOCKER) tag $(NAMESPACE)/mero-devel:$(CENTOS_RELEASE) \
				  $(NAMESPACE)/mero-devel:eos
	$(DOCKER) tag $(NAMESPACE)/mero-devel:eos \
				  $(NAMESPACE)/mero/mero:eos

all: latest  7.6  7.5  7.3  sage  eos

INAME = $(@:%-image=%)

centos-vault-image centos-sage-image centos-eos-image:
	cd $(INAME) \
	&& $(DOCKER) build . \
			    --build-arg CENTOS_RELEASE=$(CENTOS_RELEASE) \
			    --build-arg LUSTRE_VERSION=$(LUSTRE_VERSION) \
			    -t $(NAMESPACE)/centos:$(CENTOS_RELEASE) \
			    -t $(NAMESPACE)/centos:$(basename $(CENTOS_RELEASE)) \
			    $(DOCKER_OPTS)

centos-image:
	cd $(INAME) \
	&& $(DOCKER) build . \
			    --build-arg CENTOS_RELEASE=$(CENTOS_RELEASE) \
			    --build-arg LUSTRE_VERSION=$(LUSTRE_VERSION) \
			    -t $(NAMESPACE)/$(INAME):$(CENTOS_RELEASE) \
			    -t $(NAMESPACE)/$(INAME):$(basename $(CENTOS_RELEASE)) \
			    $(DOCKER_OPTS)

# Using `tar` as a workaround for symlink de-reference to pass them to `docker build`
lustre-client-image: TAG = $(CENTOS_RELEASE)-$(LUSTRE_VERSION)
mero-devel-image:    TAG = $(CENTOS_RELEASE)
lustre-client-image mero-devel-image:
	cd $(INAME) \
	&& tar -ch . \
	   | $(DOCKER) build \
		--build-arg CENTOS_RELEASE=$(CENTOS_RELEASE) \
		--build-arg LUSTRE_VERSION=$(LUSTRE_VERSION) \
		-t $(NAMESPACE)/$(INAME):$(TAG) \
		$(DOCKER_OPTS) -

#lustre-client-sage-image:
#	cd $(INAME) \
#	&& $(DOCKER) build . -t $(NAMESPACE)/lustre-client:$(CENTOS_RELEASE)-$(LUSTRE_VERSION) $(DOCKER_OPTS)

push: name := [clm]*
push: tag  := *
push:
	@for img in $$(docker images --filter=reference='$(NAMESPACE)/$(name):$(tag)' \
				    --format '{{.Repository}}:{{.Tag}}' \
			| grep -v none | sort) \
		    $$(docker images --filter=reference='$(NAMESPACE)/mero/mero:$(tag)' \
				    --format '{{.Repository}}:{{.Tag}}' \
			| grep -v none | sort) ; \
	do \
		echo "---> $$img" ; \
		$(DOCKER) push $$img ; \
	done

clean: name := [clm]*
clean: tag  := *
clean:
	@for img in $$(docker images --filter=reference='$(NAMESPACE)/$(name):$(tag)' \
				    --format '{{.Repository}}:{{.Tag}}') \
		    $$(docker images --filter=reference='$(NAMESPACE)/mero/mero:$(tag)' \
				    --format '{{.Repository}}:{{.Tag}}') ; \
	do \
		echo "---> $$img" ; \
		$(DOCKER) rmi $$img ; \
	done

help:
	@echo 'Build targets:'
	@echo '  latest          - build images based on the latest CentOS snapshot,'
	@echo '                    and latest supported Lustre version, this is the'
	@echo '                    default, same as just `make`'
	@echo '  7.6             - build images based on CentOS 7.6.1810 snapshot and'
	@echo '                    Lustre 2.10.8'
	@echo ''
	@echo '  7.5             - build images based on CentOS 7.5.1804 snapshot and'
	@echo '                    Lustre 2.10.5'
	@echo ''
	@echo '  7.3             - build images based on CentOS 7.3.1611 snapshot and'
	@echo '                    Lustre 2.9.0 (SAGE VM)'
	@echo ''
	@echo '  sage            - build images based on CentOS 7.3.1611 snapshot with'
	@echo '                    kerne 3.10.0_514.16.1 and Lustre 2.11.53 (SAGE Cluster)'
	@echo ''
	@echo 'Publish targets:'
	@echo "  push            - upload local $(NAMESPACE)/* images to docker hub repository"
	@echo ''
	@echo 'Clean targets:'
	@echo "  clean           - remove local $(NAMESPACE)/* images"
