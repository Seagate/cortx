if [ -n "$BASH_VERSION" ] ; then
    if [[ ! $PATH =~ /opt/rh/sclo-git212/ ]] ; then
        [[ -n $MANPATH ]]  || export MANPATH=:
        [[ -n $PERL5LIB ]] || export PERL5LIB=:
        source /opt/rh/sclo-git212/enable
    fi
fi
