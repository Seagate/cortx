/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF XYRATEX TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF XYRATEX TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF XYRATEX LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF XYRATEX'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A XYRATEX REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original author: Igor Vartanov
 * Original creation date: 10/2/2014
 */

/**
   @page FDMI-DLD FDMI Detailed Design

   - @ref FDMI-DLD-ovw
   - @ref FDMI-DLD-def
   - @ref FDMI-DLD-req
   - @ref FDMI-DLD-depends
   - @ref FDMI-DLD-highlights
   - @subpage FDMI-DLD-fspec "Functional Specification" <!-- Note @subpage -->
   - @ref FDMI-DLD-lspec
     - @ref FDMI-DLD-lspec-comps
     - @ref FDMI-DLD-lspec-fdmi-service
     - @ref FDMI-DLD-lspec-fdmi-source
     - @ref FDMI-DLD-lspec-fdmi-source-dock
     - @ref FDMI-DLD-lspec-fdmi-filter
     - @ref FDMI-DLD-lspec-fdmi-plugin-dock
     - @ref FDMI-DLD-lspec-numa
   - @ref FDMI-DLD-conformance
   - @ref FDMI-DLD-ut
   - @ref FDMI-DLD-st
   - @ref FDMI-DLD-O
   - @ref FDMI-DLD-ref
   - @ref FDMI-DLD-impl-plan


   @section FDMI-DLD-ovw Overview
   The document is intended to specify the detailed level design for of Mero
   FDMI interface. FDMI is a part of Mero product. FDMI provides interface for
   Mero plugins and allows Mero scale horizontally extending the features and
   capabilities of the system.

   @section FDMI-DLD-def Definitions

   - FDMI - File data manipulation interface
   - FDMI source - Mero entity that provides data to be distributed
   - FDMI plugin - An application that uses FDMI API to access Mero source data
   - FDMI source dock - FDMI subsystem responsible for handling FDMI records,
     posted by source
   - FDMI plugin dock - FDMI subsystem responsible for providing FDMI API to
     plugin application
   - FDMI record - FDMI data, posted by source.
   - FDMI record type - FDMI entity that clearly defines FDMI data type.
   FDMI source provides records of a particular type.
   - FDMI filter - Set of rules, defined by plugin, to filter out FDMI records.


   @section FDMI-DLD-req Requirements
   The requirements are fully described in @ref FDMI-DLD-ref-HLD "[0]"
   @b TBD

   @section FDMI-DLD-depends Dependencies
   @b TBD

   @section FDMI-DLD-highlights Design Highlights
   @b TBD

   @section FDMI-DLD-lspec Logical Specification

   - @ref FDMI-DLD-lspec-comps
   - @ref FDMI-DLD-lspec-fdmi-service
   - @ref FDMI-DLD-lspec-fdmi-source
     - @ref FDMI-DLD-lspec-fdmi-source-impl
   - @ref FDMI-DLD-lspec-fdmi-source-dock
     - @ref FDMI-DLD-lspec-fdmi-source-reg
     - @ref FDMI-DLD-lspec-fdmi-source-rec-post
     - @ref FDMI-DLD-lspec-fdmi-source-source-dock-fom
   - @ref FDMI-DLD-lspec-fdmi-filter
      - @ref FDMI-DLD-lspec-fdmi-filter-d
      - @ref FDMI-DLD-lspec-fdmi-filter-c
      - @ref FDMI-DLD-lspec-fdmi-filter-evaluator
   - @ref FDMI-DLD-lspec-fdmi-plugin-dock
     - @ref FDMI-DLD-lspec-fdmi-plugin-dock-reg
     - @ref FDMI-DLD-lspec-fdmi-plugin-dock-papi
     - @ref FDMI-DLD-lspec-fdmi-plugin-dock-fom
     - @ref FDMI-DLD-lspec-fdmi-plugin-impl
   - @ref FDMI-DLD-lspec-numa


   @subsection FDMI-DLD-lspec-comps Component Overview
   FDMI consists of the following components:
   - FDMI service
   - FDMI source
   - FDMI source dock
   - FDMI plugin dock

   @subsection FDMI-DLD-lspec-fdmi-service FDMI service
   FDMI service runs as a part of Mero instance. FDMI service stores context
   data for both FDMI source dock and FDMI plugin dock. FDMI service is
   initialized and started on Mero instance start up, FDMI Source dock and FDMI
   plugin dock are both initialised on the service start unconditionally.


   @subsubsection FDMI-DLD-lspec-fdmi-source FDMI source

   FDMI source instance main task is to post FDMI records of a specific type to
   FDMI source dock for further analysis, Only 1 FDMI source instance with a
   specific type should be registered in a Mero process: FDMI record type
   uniquely identifies FDMI source instance. A list of FDMI record types
   (::m0_fdmi_rec_type_id): - FOL record type - ADDB record type - TBD

    FDMI source instance provides the following interface functions for FDMI
    source dock to handle FDMI records:
    - Get source specific value, m0_fdmi_src::fs_node_eval()
    - Increase record reference counter, m0_fdmi_src::fs_get()
    - Decrease record reference counter, m0_fdmi_src::fs_put()
    - FDMI record processing start, m0_fdmi_src::fs_begin()
    - FDMI record processing complete, m0_fdmi_src::fs_end()

    FDMI source provides records of specific FDMI record type
    m0_fdmi_rec_type. FDMI record type is identified by id
    ::m0_fdmi_rec_type_id. FDMI source provides Xcode functions
    m0_fdmi_src::fs_encode() and m0_fdmi_src::fs_decode().

   @subsubsection FDMI-DLD-lspec-fdmi-source-impl FDMI source implementation guideline

   FDMI source implementation depends on data domain. Specific FDMI source type
   stores
   - FDMI generic source interface m0_fdmi_src
   - FDMI specific source context data (source private data)

   FDMI Source must:
   - define FDMI record type ID (see ::m0_fdmi_rec_type_id)
   - implement record type operations (see m0_fdmi_src)
   - implement required FDMI callbacks (see m0_fdmi_src for list of
					callbacks)
   - allocate source structure with m0_fdmi_source_alloc() call
   - register it with FDMI (call to m0_fdmi_source_register(), pass on source)
   - post FDMI records using the callback returned by
     m0_fdmi_source_register() -- see m0_fdmi_src::fs_record_post(),
     m0_fdmi_src_dock_pops::fdmi_post_record.  It's recommended to use macro
     M0_FDMI_SOURCE_POST_RECORD() for that.
   - deregister the source with FDMI on finalisation using
   m0_fdmi_source_deregister()
   - free the allocated source structure with m0_fdmi_source_free()

   To pass FDMI record to FDMI source dock, API uses pointer to
   m0_fdmi_src_rec.  The source must include this struct as a member of some
   other structure (preferrably the one which contains the data of the record
   itself) -- for example, for FOL source, the struct is embedded into FOL
   record, m0_fol_rec.  Later on, source dock will pass the pointer back in
   all the calls, and source will be able to use container_of to obtain the
   pointer to ambient structure, and perform the operations required.  Source
   must make sure that the structure is NOT deallocated before source dock
   calls the last m0_fdmi_src::fs_put() -- otherwise it will lead to a crash.

   Now, workflow for a single FDMI record:
   - Source: issues post-record callback, passes on a pointer to src_rec.
   - FDMI: saves the pointer in internal queue of records to be processed.
   - Source: increases ref counter on this record (will be released later
						       on by FDMI).
   - ... some time passes,
   - FDMI worker picks this record from the queue and starts processing.
   - FDMI calls m0_fdmi_src::fs_begin() -- this is an indicator
     to the source that it's time to cache the record in memory for fast
     access.
   - FDMI starts filter evaluation, and will repeatedly call
     m0_fdmi_src::fs_node_eval() for each filter.
   - for each matched filter, FDMI will call m0_fdmi_src::fs_get(),
     and send the record to the plugin.
   - Once all filters are processed, FDMI calls m0_fdmi_src::fs_put()
     to balance fs_get().
   - At some moment between fs_begin() and fs_end(), FDMI
     will call m0_fdmi_src::fs_encode() (possibly multiple times) to
     generate serialized representation of this given record.
   - Once all filters are processed, and encoding compete, FDMI calls
     m0_fdmi_src::fs_end(), which is an indicator to the
     source that record data is no longer needed in memory and can be saved in
     "slow" storage (no more fs_node_eval() or fs_encode() calls).

     @note If source supports transactions and re-sending, record cannot be
     completely discarded until all plugins replied with 'release'.

   - ... some time passes
   - When done with record processing, plugins, independent from each other,
   post a 'release' request to source that originally produced the precessed
   FDMIrecord. For each 'release', FDMI calls m0_fdmi_src::fs_put().
   Total count of dec_refc calls will match count of inc_refc calls.  Once ref
   counter is zero the record can be completely discarded.

   @note

   - 'Release' request needs to be not confused with regular RPC reply. The
   reply is sent immediately when FDMI record appears registered in receiving
   plugin dock. But 'release' request is issued to source when there remains no
   plugin having the record in process.
   - If record matched no filters, there will be only one fs_put() call, see
   above, and it will happen _before_ fs_end() call.
   - During _fini phase, source dock will NOT call fs_end()/fs_put().  This
   way, source will have information on which records have to be resend on
   next startup.  Note that source must NOT free these records before source
   dock _fini call -- dock fini needs these records for finalization.

   * * * *

   For the moment FDMI FOL source is implemented as the 1st (and currently the
   only) FDMI source. FDMI FOL source provides ability for detailed FOL data
   analysis.


   @note Algorithm described below is not implemented yet.

   For FOL record specific data handling FDMI FOL record type is declared and
   registered for each specific FOL record type (example: write operation FOL
   record, set attributes FOL record, etc.)

   FDMI FOL record type context stores the following:
   - FOL operation code
   - FOL record interface

   On FDMI FOL record type FDMI record registration all its internals are
   initialized and saved as FDMI FOL record context data. Pointer to FDMI FOL
   record type is stored as a list in FDMI specific source context data.


   @subsubsection FDMI-DLD-lspec-fdmi-source-dock FDMI Source Dock
   FDMI Source dock (FDMI service), responsible for:
   - Source registration
   - Retrieving/refreshing filter set for the source
   - Input data filtration
   - Deciding on and posting notifications to filter subscribers over Mero RPC
   - Deferred input data release

   @subsubsection FDMI-DLD-lspec-fdmi-source-reg FDMI source registration

   On FDMI source registration all its internals are initialized and saved as
   FDMI generic source context data m0_fdmi_src_dock. Pointer to FDMI source
   instance is passed to FDMI source dock and saved in a list. In its turn, FDMI
   source dock provides back to FDMI source instance an interface function to
   perform FDMI record posting m0_fdmi_src_dock_pops. FDMI generic source
   context contains the following:
   - FDMI record type
   - FDMI generic source interface
   - FDMI source dock interface

   m0_fdmi_src structure stores all FDMI generic source context data. It is
   populated on source side right after being allocated providing source
   specific callbacks, as well as on FDMI side during source registartion
   providing source dock private APIs. Registered FDMI sources are stored in the
   list m0_fdmi_src_dock::fsdc_src_list @see m0_fdmi_source_register().

   @subsection FDMI-DLD-lspec-fdmi-source-rec-post FDMI record post

   Source starts with local locking data to be fed to FDMI interface, then it
   calls posting FDMI API M0_FDMI_SOURCE_POST_RECORD(). An argument to that
   post method is a pointer to m0_fdmi_src_rec. This same pointer will be
   later used by source dock when calling back to issue operations on the
   record.  The source will then need to trace the pointer back to the object
   which triggered the record post.  The suggested method to achieve this is:
   embed the m0_fdmi_src_rec into the same structure which caused the posting.
   Later on, when source dock calls back, source can use container_of to get
   original pointer to ambient structure and perform the operation.

   So once record is posted, on FDMI side, the record is added to a waiting
   queue and assigned a record id.  Posted FDMI records are stored in the list
   m0_fdmi_src_dock::fsdc_posted_rec_list. Posted records are then handled by
   FDMI source dock FOM.

   Record id is 128bit int (m0_int128).  High part, u_hi, is set to
   m0_fdmi_src_dock::fsdc_instance_id.  Lower part, u_lo, is set to a pointer
   value passed to M0_FDMI_SOURCE_POST_RECORD() -- see m0_fdmi__rec_id_gen().
   This way, the source dock does not need to keep any mapping structures --
   e.g. when 'release' comes in, and dock needs to call m0_fdmi_src::fs_put(),
   it will simply pass the u_lo as a pointer to m0_fdmi_src_rec.  To prevent
   issues on restart (e.g. source dock sends a record to plugin, then gets
   restarted, and then receives 'release' from plugin; takes u_lo and tries to
   dereference, and crashes since that pointer is no longer valid) -- to
   prevent these issues, fsdc_instance_id must be a unique ID and it must
   change on each restart.

   @subsection FDMI-DLD-lspec-fdmi-source-source-dock-fom FDMI source dock FOM
   FDMI source dock FOM implements the main control flow for FDMI source dock:
   - Takes out posted FDMI records
   - Examines filters
   - Sends notifications to FDMI plugins
   - Analyzes FDMI plugin responses

   @b Normal @b workflow

   FDMI source dock FOM remains in an idle state if no FDMI record is posted
   (FDMI record queue m0_fdmi_src_dock::fsdc_posted_rec_list is empty). If
   any FDMI record is posted, the FOM switches
   to busy state, takes out FDMI record from a queue and starts analysis. Source
   dock informs the source on FDMI record analysis start using interface
   functioin m0_fdmi_src::fs_begin()

   Before examining against all the filters, FOM requests filter list from
   filterc. On getting filter list, FOM iterates through the filter list and
   examines filters one by one.

   If no filter shows a match for the FDMI record, the record is released. To
   inform FDMI source that this record is no more needed for FDMI system, FDMI
   generic source interface function "decrease record reference counter"
   m0_fdmi_src::fs_put() is used.

   If one or more filters match the FDMI record, the record is scheduled to be
   sent to a particular FDMI node(s). If several filters matched, the following
   operations are performed to optimize data flow:

   - Send FDMI record only once to a particular FDMI node (filter provides RPC
   endpoint to communicate with).
   - Specify a list of matched filters, include only filters that are related to
   the node.
   - On receipt, FDMI plugin dock is responsible for dispatching received FDMI
   records and pass it to plugins according to specified matched filters list.

   In order to manage FDMI records transportation, the following information
   should be stored as FDMI source dock context information:

   - Relation between destination Filter Id and FDMI record id being sent to the
   specified Filter ID
     - Map <Filter Id, FDMI record id> may be used in this case
     - This information is needed to handle Corner case "Mero instance
     running FDMI plugin dock death"

   FDMI record being sent is serialized using FDMI record type interface
   function "Xcode functions" (see m0_fdmi_src)

   On sending FDMI record its reference counter is increased: FDMI generic
   source interface function "increase record reference counter"
   m0_fdmi_src::fs_get() is used.

   FDMI source dock increments internal FDMI record reference counter for the
   FDMI record being sent for each sending operation.

   On FDMI record receipt, FDMI plugin dock should answer with a reply that is
   understood as a data delivery acknowledgement. The data acknowledgment
   should be sent as soon as possible.

   On reply receive, internal FDMI record reference counter for the
   FDMI record is decremented in order to check if FDMI record notification is
   replied by all the plugins. If internal reference counter becomes 0,
   FDMI source is informed on record processing complete using API call
   m0_fdmi_src::fs_end(). Then FDMI record is removed from
   the FDMI source dock communication context.

   After FDMI record is handled by all involved plugins, FDMI plugin dock should
   send FDMI record release request to the FDMI record originator (FDMI source
   dock). On receiving this request, FDMI source dock removes appropriate pair
   <Filter Id, FDMI record id> from its context and informs FDMI source that the
   record is released. FDMI generic source interface function "decrease
   record reference counter" m0_fdmi_src::fs_put() is used for this
   purpose. If FDMI source reference counter for a particular FDMI record
   becomes 0, FDMI source may release this FDMI record.

   FDMI source dock normal workflow is described on the picture below.

   @image html "../../fdmi/img/FDMI-source-dock--FDMI-FOM.png" "FDMI Source Dock FOM"
   <!-- PNG image width is 800 -->

   @b Corner @b cases

   Special handling should be applied for the following corner cases:
   - Mero instance running "FDMI plugin dock" death
   - FDMI filter is disabled
   Mero instance running "FDMI plugin dock" death may cause 2 cases:
   - RPC error while sending FDMI record to a FDMI source dock. No RPC reply
   received.
   - No "FDMI record release" request is received from FDMI plugin dock

   @note Corner cases handling needs HA support, not implemented now.
   For now they are handled using basic mechanisms like timeout, etc.
   Adding HA support allows handling them in a more straightforward way.

   @subsection FDMI-DLD-lspec-fdmi-filter FDMI Filter
   FDMI filter is a set of rules, defined by plugin, to filter out FDMI
   records. Filters are registered and propagated over Mero system using
   features and API provided by FilterC and FilterD subsystems.

   @subsection FDMI-DLD-lspec-fdmi-filter-d FDMI filterD
   FDMI plugin creates filter in order to specify criteria for FDMI records it
   is interested in. FDMI filter service (filterD) maintains central database of
   the FDMI filters existing in Mero cluster.  There is only one (possibly
   duplicated) mero instance with filterD service in the whole Mero
   cluster. FilterD provides to users read/write access to its database via RPC
   requests.

   FilterD service is started as a part of chosen for this purpose mero
   instance. Address of filterD service endpoint is stored in confd
   database. FilterD database is empty after cluster initialisation. The
   database won't be typically empty after filterD restart for any reason.

   FilterD database is protected by a distributed read/write lock. When filterD
   database should be changed, filterD service acquires exclusive write lock
   from Resource Manager (RM), thus invalidating all read locks held by database
   readers. This mechanism is used to notify readers about filterD database
   changes, forcing them to re-read database content afterwards.

   There are two types of filterD users:
   - FDMI plugin dock (@ref FDMI-DLD-lspec-fdmi-plugin-dock)
   - FDMI filter client (filterC) (@ref FDMI-DLD-lspec-fdmi-filter-c)

   FDMI filter description stored in database is represented by structure
   @ref m0_conf_fdmi_filter

   FDMI plugin dock can issue following requests:
   - Add filter with provided description
   - Remove filter by filter ID
   - Activate filter by filter ID
   - Deactivate filter by filter ID
   - Remove all filters by FDMI plugin dock RPC endpoint

   Also there are other events that cause some filters deactivation in database:
   - HA notification about node death

   Filters stored in database are grouped by FDMI record type ID they are
   intended for.
   FilterC clients can issue following queries to filterD:
   - Get all FDMI record type ID's known to filterD
   - Get all FDMI filters registered for specific FDMI record type ID

   @note Currently FilterD doesn't exist as a separate module, confd is used
   instead.

   @subsection FDMI-DLD-lspec-fdmi-filter-c FDMI filterC
   FilterC is a part of Mero instance that caches locally filters obtained from
   filterD. FilterC is initialized by FDMI source dock service at its startup.
   Also, filterC have a channel in its context which is signaled when some
   filter state is changed from enabled to disabled.

   FilterC achieves local cache consistency with filterD database content by
   using distributed read/write lock mechanism. FilterD database change is the
   only reason for filterC local cache update. HA notifications about filter or
   node, running confd, death - are ignored by filterC.

   @note Currently FilterC implementation is based on confc.

   @subsection FDMI-DLD-lspec-fdmi-filter-evaluator FDMI filter evaluator
   FDMI filter evaluator is responsible for filter expression calculation.
   Filter expression is defined in FDMI filter (@ref m0_fdmi_filter) and is
   represented by tree structure. FDMI filter evaluator traverses filter
   expression tree and calculates a result. The result is always boolean and
   indicates whether posted FDMI record notification matches against filter
   or not.

   @subsection FDMI-DLD-lspec-fdmi-plugin-dock FDMI Plugin dock
   FDMI Plugin dock is responsible for:
   - Plugin registration in FDMI instance
   - Filter registration in Mero Filter Database
   - Listening to notifications coming over RPC
   - Payload processing

   @subsection FDMI-DLD-lspec-fdmi-plugin-dock-reg FDMI plugin registration
   There is no clearly defined procedure for plugin registration. The only
   mandatory step is obtaining @ref FDMI-DLD-lspec-fdmi-plugin-dock-papi
   "private plugin dock API".

   As long as FDMI record delivery is stipulated by record matching to certain
   filtering conditions plugin needs to register some set of filter definitions
   specific to plugin's operational logic. Plugin is free to register any subset
   of filter definitions any time it needs those be put in use.

   Newly added filter definition remains inactive until being activated
   explicitly. Filter activation is a bulk operation allowing
   activate/deactivate some subset of filters at a time.

   Unlike to its registration, plugin is expected to deregister all filter
   definitions it ever added during the current session.

   @subsection FDMI-DLD-lspec-fdmi-plugin-dock-papi FDMI plugin dock private API
   Private API provides the following functionality for plugin writers:
   - register filter - registering single filter definition
   - enable filters - enabling/disabling a bunch of filter definitions
   - release fdmi rec - indicating the record can be released on source side
   - deregister plugin - unregistering all previously registered filter
   definitions

   The API is intended to be not used directly. Instead, it is obtained
   explicitly by plugin side using m0_fdmi_plugin_dock_api_get().

   @see struct m0_fdmi_pd_if
   @see m0_fdmi_plugin_dock_api_get()

   @subsection FDMI-DLD-lspec-fdmi-plugin-dock-fom FDMI plugin dock FOM Received
   FDMI record goes directly to Plugin Dock's FOM. At this time incoming RPC
   connection endpoint address needs to be stored in communication context being
   associated with fdmi record id. Immediately on FOM initialisation step RPC
   reply is sent confirming fdmi record delivery.

   Per filter id, corresponding plug-in is called feeding it with fdmi data,
   fdmi record id and filter id specific to the plug-in. Every successful
   plug-in feed results in incrementing fdmi record id reference counter. When
   done with the ids, FOM needs to test if at least a single feed succeeded. In
   case it was no success, i.e. there was not a single active filter
   encountered, or plug-ins never confirmed fdmi record acceptance, the fdmi
   record has to be released immediately.

   Plug-in decides on its own when to report fdmi original record to be released
   by Source. It calls Plug-in Dock about releasing particular record identified
   by fdmi record id. In context of the call fdmi record reference counter is
   decremented locally, and in case the reference counter gets to 0, the
   corresponding Source is called via RPC to release the record (see Normal
   workflow, FDMI Source Dock: Release Request from Plug-in).

   @image html "../../fdmi/img/FDMI-plugin-dock--On-FDMI-Record.png" "FDMI plugin dock FOM"

   FOM being initialised prepares context for filter array iterative
   processing. On every iteration plugin is fed with the FDMI record along with
   current filter id. When done with the array, FOM deincrements record
   registration and finalises itself.

   @verbatim
              |
              |
              V
         INITIALISED
              |
              |  pdock_fom_tick__init()
              |
              |
              |      /-------\
              |      |       |
              V      V       |
    FEED_PLUGINS_WITH_REC    |
              |              |
              | ++pf_pos     |  pf_pos < fmf_count
              |              |
              |              |
              |              |
              |  pdock_fom_tick__feed_plugin_with_rec()
              |              |
              |              |
              +--------------/
              |
              |
              |  pf_pos == fmf_count
              |
              |
              |
              V
       FINISH_WITH_REC
              |
              |
              |
              |  pdock_fom_tick__finish_with_rec()
              |
              |
              V
          FINALISED


   @endverbatim

   @subsection FDMI-DLD-lspec-fdmi-plugin-impl FDMI plugin implementation guideline

   The main logic behind making use of a FDMI plug-in is a subscription to some
   events in sources that comply with conditions described in filters that
   plug-in registers at its start. In case some source record matches with at
   least one filter, the source-originated record is routed to corresponding
   plug-in.

   Plug-in responsibilities

   During standard initialization workflow plug-in:
   - Obtains private Plug-in Dock callback interface
   - Registers set of filters, where filter definition:
     - Identifies FDMI record type to be watched
     - Provides filter id value unique across entire Mero cluster
     - Provides plug-in callback interface
     - Provides description of condition(s) the source record to meet to invoke
   notification

     During active subscription workflow looks like following:
     - Plug-in is called back with:
       - FDMI record id
       - FDMI record data
       - Filter id indicating the filter that signaled during the original
     source record processing
     - Plug-in must keep trace of FDMI record (identified by FDMI record id
     globally unique across the Mero cluster) during its internal processing.
     - Plug-in must return from the callback as quick as possible to not block
     other callback interfaces from being called. Plug-in writers must take into
     account the fact that several plug-ins may be registered simultaneously,
     and therefore, must do their best to provide smooth cooperation among
     those.
     - However plug-in is allowed to take as much time as required for FDMI
     record processing. During the entire processing the FDMI record remains
     locked in its source.
     - When done with the record, plug-in is responsible for the record release.
     - Plug-in is allowed to activate/deactivate any subset of its registered
     filters. The decision making is entirely on plug-in's side.
     - The same way plug-in is allowed to de-register and quit any time it
     wants. The decision making is again entirely on plug-in's
     side. After de-registering itself the plug-in is not allowed to call
     private FDMI Plug-in Dock in part of filter activation/deactivation as well
     as fdmi record releasing. The said actions become available only after
     registering filter set another time.

   @subsection FDMI-DLD-lspec-numa NUMA optimizations
   @b TBD

   <hr>
   @section FDMI-DLD-conformance Conformance
   @b TBD

   <hr>
   @section FDMI-DLD-ut FDMI Unit Tests
   - Verify FDMI source dock
   - Verify FDMI plugin dock
   - Verify FDMI filter subsystem
   - Verify FOL FDMI source implementation
   - Verify FDMI Connection Pool module

   @subsection FDMI-DLD-ut-fdmi-sd FDMI source dock unit tests
   - Source dock
     - Verify registering/deregistering sources in source dock
     - Verify FDMI record posting interface
     - Verify registered source callbacks are called in right order
       and with right arguments
   - Source dock FOM
     - Verify FOM start/stop
     - Verify FOM fetches posted FDMI records
     - Verify FOM acquire necessary filters from FilterC and apply them
       to FDMI records being processed
     - Verify that if filter is matched, then FDMI record is sent to plugin
       dock over RPC
   - Source dock release FOM
     - FOM is created on 'Release FDMI record notification' FOP
     - Registered source callback for decrementing reference counter is called

   @test m0_fdmi_source_register() assigns correct private callbacks
   (see m0_fdmi_src::fs_record_post())

   @test m0_fdmi_source_deregister() clears private callbacks in source
   registration structure

   @test m0_fdmi__record_post() puts FDMI record into list and wake-ups source
   dock FOM if list was empty

   @test m0_fdmi__handle_reply() calls registered m0_fdmi_src::fs_put()
   in case of RPC packet sending failure

   @test m0_fdmi__handle_release() calls registered
   m0_fdmi_src::fs_put()

   @test m0_fdmi__src_dock_fom_start() starts FOM correctly

   @test m0_fdmi__src_dock_fom_stop() wakes up FOM so it can stop itself

   @test process_fdmi_rec() calls registered source callback
   m0_fdmi_src::fs_begin() and applies all filters stored in
   filterC for this record type, calling registered source callback
   m0_fdmi_src::fs_node_eval() when necessary. For each matched filter
   registered source callback m0_fdmi_src::fs_get() is called.

   @test sd_fom_process_matched_filters() creates 'FDMI record notification'
   FOPs for all matched filters and sends them over RPC.

   @test fdmi_rr_fom_tick calls registered source callback
   m0_fdmi_src::fs_put() and posts reply with
   m0_fop_fdmi_rec_release_reply::frrr_rc set to 0

   @subsection FDMI-DLD-ut-fdmi-pd FDMI plugin dock unit tests
   - Plugin dock
     - Verify plugin dock start/stop routines
     - Verify FDMI filter addition correct and incorrect data cases
     - Verify FDMI filter enabling/disabling with registered and non-existen ids
     - Verify FDMI plugin deregistration
     - Verify FDMI record release commands with:
       - registered record id
         - ref counter greater than 1
	 - ref counter equal to 1
       - unknown record id
     - Verify all memory allocation failure cases
     - Verify FDMI filter lookup failure cases
   - Plugin dock FOM
     - Verify FOM creation with FDMI record notification FOP
       - complete notification data
       - incomplete/crippled notification data
     - Verify FOM finalisation
     - Verify FOM state transitions flow
     - Verify automatic FDMI record release request posting in case there was no
   record
   acceptance on plugin side
     - Verify all memory allocation failure cases
     - Verify FDMI record lookup failure cases
     - Verify FDMI filter lookup failure cases

   @test m0_fdmi__plugin_dock_init() and m0_fdmi__plugin_dock_start() initialise
   plugin dock correctly

   @test m0_fdmi__plugin_dock_stop() and m0_fdmi__plugin_dock_fini() finalise
   plugin dock correctly unregistering filter and record registrations if any
   remained to the moment

   @test Plugin correctly obtains private API interface with
   m0_fdmi_plugin_dock_api_get()

   @test register_filter() successfully registers filter with correct filter
   attributes setting filter registration to deactivated state

   @test register_filter() fails with incorrect filter attributes

   @test enable_filters() correctly activates/deactivates filter registration
   entries with known filter ids, and being provided with unknown filter ids
   successfully ignores those

   @test release_fdmi_rec() successfully finds record with know id and
   decrements its ref counter causing release request when counter reaches to
   zero resulting in pdock_record_release() call

   @test deregister_plugin() successfully unregisters all known filter
   registrations with ids provided by plugin

   @test m0_fdmi__pdock_fdmi_record_register() is able to create record
   registration entry with complete and correctly built FOP

   @test pdock_fom_create() is able to successfully create FOM being provided
   with complete and correctly built FOP and register FDMI record for further
   processing

   @test pdock_fom_create() correctly terminates when having FOP information not
   enough to register FDMI record, or having memory allocation
   failed. Disregarding FOM creation cancelling release request will be posted
   anyway in case FDMI record id is identifiable in FOP.

   @test pdock_fom_tick__init() correctly replies to source over RPC and
   initialises record processing context

   @test pdock_fom_tick__feed_plugin_with_rec() correctly iterates through
   filter ids, calls plugin back and is able to find existent FDMI record
   registeration each time when plugin accepts the record

   @test pdock_fom_tick__finish_with_rec() is able to find the processed record
   and correctly unregister the one


   @subsection FDMI-DLD-ut-fdmi-filter FDMI filter subsystem unit tests
   - FilterC
     - Verify FilterC start/sop routines
     - Verify routines to connect to confd and load filters definitions
   - Filter Evaluation
     - Verify routines to construct filter (init/fini/create node)
     - Verify routines to apply filter to an FDMI record (evaluate)
     - Verify routines to convert filter to/from string
     - Verify that filter evaluator is able to handle source-specific operations

   @test m0_filterc_ops::fco_start is called and successfully completes when
   FDMI service is started (which means filterc has started successfully).

   @test m0_filterc_start() is able to connect to confd and load filters from
   it.

   @test m0_filterc_stop() is able to properly finalise filterc.

   @test m0_fdmi_eval_flt() evaluates results properly (for each supported
   operator and each type, including error cases e.g. unsupported operand
   types).

   @test m0_fdmi_eval_flt() properly handles source-specific operations,
   specified using m0_fdmi_eval_add_op_cb().

   @test m0_xcode_print() and m0_xcode_read() are able to digest filter
   structures (definitions).

   @test m0_fdmi_flt_node_to_str() and m0_fdmi_flt_node_from_str() are able to
   serialize/deserialize filter definitions properly.


   @subsection FDMI-DLD-ut-fdmi-fol FOL FDMI source implementation unit tests

   - Verify that FOL source init/fini routines work properly.
   - Run FOL source interface functions in the same order they would be
     called by FDMI source dock, and make sure they behave properly.
   - Make sure encode/decode do not corrupt data (no data loss, and no added
     garbage).
   - Check error cases for FOL source interface functions.

   @test m0_fol_fdmi_src_init() call followed by m0_fol_fdmi_src_fini() call
   work as expected.

   @test ffs_op_node_eval() extract value as expected.

   @test ffs_op_get() and ffs_op_put() do not modify counters.

   @test ffs_op_encode() succeeds and encodes data properly.

   @test ffs_op_decode() succeeds and decoded data matches original
   encoded record.

   @test ffs_op_begin() does not modify counters.

   @test ffs_op_end() decreases transaction counter by one (m0_be_tx_put()).

   @test m0_fol_fdmi_src_fini() is able to handle case when there are
   "un-processed" FDMI records.

   @test m0_fol_fdmi_post_record() calls required Source Dock methods, saves
   record into internal hash, and calls m0_be_tx_get().


   @subsection FDMI-DLD-ut-fdmi-cp FDMI Connection Pool module implementation
   unit tests

   - FDMI Connection Pool module
     - Verify that init/fini routines work properly.
     - Verify that RPC connect/disconnect works.
     - Verify that error cases are handled.

   @test m0_fdmi_conn_pool_init() initializes internals properly.

   @test m0_fdmi_conn_pool_get() creates new connection if existing not found.

   @test m0_fdmi_conn_pool_get() returns new session on existing connection if
   found.

   @test m0_fdmi_conn_pool_get() and m0_fdmi_conn_pool_put() handle counters
   properly.

   @test m0_fdmi_conn_pool_fini() succeeds.


   @section FDMI-DLD-st System Tests
   FDMI simple echo plugin is implemented as a system test.

   @section FDMI-DLD-O Analysis
   @b TBD

   @section FDMI-DLD-ref References
   - @anchor FDMI-DLD-ref-HLD [0] <a
   href="https://docs.google.com/document/d/1xj5BvLeWUBj1_0mwITa_0irFJf9TqBQgllpKZkjAds0/edit#">
   Mero FDMI HLD</a>

   @section FDMI-DLD-impl-plan Implementation Plan
   Implementation plan is specified as a part of HLD @ref ADDB-DLD-ref-HLD "[0]"
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_FDMI
#include "lib/trace.h"

#include "lib/tlist.h"
#include "fdmi/fdmi.h"
#include "fdmi/plugin_dock.h"
#include "fdmi/plugin_dock_internal.h"
#include "fdmi/source_dock.h"
#include "fdmi/source_dock_internal.h"
#include "fdmi/filter.h"
#include "fdmi/filter_xc.h"

static struct m0_fdmi_src_dock  fdmi_global_src_dock;

M0_INTERNAL int m0_fdmi_init(void)
{
	int rc;

	M0_ENTRY();
	m0_xc_fdmi_filter_init();
	m0_fdmi_source_dock_init(&fdmi_global_src_dock);
	rc = m0_fdmi__plugin_dock_init();

	return M0_RC(rc);
}

M0_INTERNAL void m0_fdmi_fini(void)
{
	M0_ENTRY();

	m0_fdmi_source_dock_fini(&fdmi_global_src_dock);
	m0_fdmi__plugin_dock_fini();
	m0_xc_fdmi_filter_fini();

	M0_LEAVE();
}

M0_INTERNAL struct m0_fdmi_src_dock *m0_fdmi_src_dock_get(void)
{
	return &fdmi_global_src_dock;
}

#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
