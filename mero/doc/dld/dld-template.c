/* -*- C -*- */
/*
 * COPYRIGHT 2012 XYRATEX TECHNOLOGY LIMITED
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
 * Original author: Carl Braganza <Carl_Braganza@xyratex.com>
 * Original creation date: 09/26/2011
 */

/**
   @page DLD Mero DLD Template

   - @ref DLD-ovw
   - @ref DLD-def
   - @ref DLD-req
   - @ref DLD-depends
   - @ref DLD-highlights
   - @subpage DLD-fspec "Functional Specification" <!-- Note @subpage -->
   - @ref DLD-lspec
      - @ref DLD-lspec-comps
      - @ref DLD-lspec-sc1
      - @ref DLD-lspec-state
      - @ref DLD-lspec-thread
      - @ref DLD-lspec-numa
   - @ref DLD-conformance
   - @ref DLD-ut
   - @ref DLD-st
   - @ref DLD-O
   - @ref DLD-ref
   - @ref DLD-impl-plan


   <hr>
   @section DLD-ovw Overview
   <i>All specifications must start with an Overview section that
   briefly describes the document and provides any additional
   instructions or hints on how to best read the specification.</i>

   This document is intended to be a style guide for a detail level
   design specification, and is designed to be viewed both through a
   text editor and in a browser after Doxygen processing.

   You can use this document as a template by deleting all content,
   while retaining the sections referenced above and the overall
   Doxygen structure of a page with one or more component modules.
   You @b <i>must</i> change the Doxygen reference tags used in
   @@page, @@section, @@subsection and @@defgroup examples when
   copying this template, and adjust @@ref and @@subpage references in
   the table of contents of your own document accordingly.

   Please provide a table of contents for the major sections, as shown above.
   Please use horizontal ruling exactly as shown in this template, and do not
   introduce additional lines.

   It is recommended that you retain the italicized instructions that
   follow the formally recognized section headers until at least the
   DLD review phase.  You may leave the instructions in the final
   document if you wish.

   It is imperative that the document be neat when viewed textually
   through an editor and when browsing the Doxygen output - it is
   intended to be relevant for the entire code life-cycle.  Please
   check your grammar and punctuation, and run the document through a
   spelling checker.  It is also recommended that you run the source
   document through a paragraph formatting tool to produce neater
   text, though be careful while doing so as text formatters do not
   understand Doxygen syntax and significant line breaks.

   Please link your DLD to the index of all detailed designs maintained
   in @ref DLDIX "Detailed Designs". <!-- doc/dld/dld-index.c -->

   <b>Purpose of a DLD</b> @n
   The purpose of the Detailed Level Design (DLD) specification of a
   component is to:
   - Refine higher level designs
   - To be verified by inspectors and architects
   - To guide the coding phase

   <b>Location and layout of the DLD Specification</b> @n
   The Mero project requires Detailed Level Designs in the source
   code itself.  This greatly aids in keeping the design documentation
   up to date through the lifetime of the implementation code.

   The main DLD specification shall primarily be located in a C file
   in the component being designed.  The main DLD specification can be
   quite large and is probably not of interest to a consumer of the
   component.

   It is @a required that the <b>Functional Specification</b> and
   the <b>Detailed Functional Specification</b> be located in the
   primary header file - this is the header file with the declaration
   of the external interfaces that consumers of the component's API
   would include.  In case of stand alone components, an appropriate
   alternative header file should be chosen.

   <b>Structure of the DLD</b> @n
   The DLD specification is @b required to be sectioned in the
   specific manner illustrated by the @c dld-sample.c and
   @c dld-sample.h files.  This is similar in structure and
   purpose to the sectioning found in a High Level Design.

   Not all sections may be applicable to every design, but sections
   declared to be mandatory may not be omitted.  If a mandatory
   section does not apply, it should clearly be marked as
   non-applicable, along with an explanation.  Additional sections or
   sub-sectioning may be added as required.

   It is probably desirable to split the Detailed Functional
   Specifications into separate header files for each sub-module of
   the component.  This example illustrates a component with a single
   module.

   <b>Formatting language</b> @n
   Doxygen is the formatting tool of choice.  The Doxygen @@page
   format is used to define a separate top-level browsable element
   that contains the body of the DLD. The @@section, @@subsection and
   @@subsubsection formatting commands are used to provide internal
   structure.  The page title will be visible in the <b>Related Pages</b>
   tab in the main browser window, as well as displayed as a top-level
   element in the explorer side-bar.

   The Functional Specification is to be located in the primary header
   file of the component in a Doxygen @@page that is referenced as a
   @@subpage from the table of contents of the main DLD specification.
   This sub-page shows up as leaf element of the DLD in the explorer
   side-bar.

   Detailed functional specifications follow the Functional
   Specification, using Doxygen @@defgroup commands for each component
   module.

   Within the text, Doxygen commands such as @@a, @@b, @@c and @@n are
   preferred over equivalent HTML markup; this enhances the readability
   of the DLD in a text editor.  However, visual enhancement of
   multiple consecutive words does require HTML markup.

   <hr>
   @section DLD-def Definitions
   <i>Mandatory.
   The DLD shall provide definitions of the terms and concepts
   introduced by the design, as well as the relevant terms used by the
   specification but described elsewhere.  References to the
   M0 Glossary and the component's HLD are permitted and encouraged.
   Agreed upon terminology should be incorporated in the glossary.</i>

   Previously defined terms:
   - <b>Logical Specification</b> This explains how the component works.
   - <b>Functional Specification</b> This is explains how to use the component.

   New terms:
   - <b>Detailed Functional Specification</b> This provides
     documentation of ll the data structures and interfaces (internal
     and external).
   - <b>State model</b> This explains the life cycle of component data
     structures.
   - <b>Concurrency and threading model</b> This explains how the the
     component works in a multi-threaded environment.

   <hr>
   @section DLD-req Requirements
   <i>Mandatory.
   The DLD shall state the requirements that it attempts to meet.</i>

   They should be expressed in a list, thusly:
   - @b R.DLD.Structured The DLD shall be decomposed into a standard
   set of section.  Sub-sections may be used to further decompose the
   material of a section into logically disjoint units.
   - @b R.DLD.What The DLD shall describe the externally visible
   data structures and interfaces of the component through a
   functional specification section.
   - @b R.DLD.How The DLD shall explain its inner algorithms through
   a logical specification section.
   - @b R.DLD.Maintainable The DLD shall be easily maintainable during
   the lifetime of the code.

   <hr>
   @section DLD-depends Dependencies
   <i>Mandatory. Identify other components on which this specification
   depends.</i>

   The DLD specification style guide depends on the HLD and AR
   specifications as they identify requirements, use cases, @a \&c.

   <hr>
   @section DLD-highlights Design Highlights
   <i>Mandatory. This section briefly summarizes the key design
   decisions that are important for understanding the functional and
   logical specifications, and enumerates topics that need special
   attention.</i>

   - The DLD specification requires formal sectioning to address specific
   aspects of the design.
   - The DLD is with the code, and the specification is designed to be
   viewed either as text or through Doxygen.
   - This document can be used as a template.

   <hr>
   @section DLD-lspec Logical Specification
   <i>Mandatory.  This section describes the internal design of the component,
   explaining how the functional specification is met.  Sub-components and
   diagrams of their interaction should go into this section.  The section has
   mandatory subsections created using the Doxygen @@subsection command.  The
   designer should feel free to use additional sub-sectioning if needed, though
   if there is significant additional sub-sectioning, provide a table of
   contents here.</i>

   - @ref DLD-lspec-comps
   - @ref DLD-lspec-sc1
      - @ref DLD-lspec-ds1
      - @ref DLD-lspec-sub1
      - @ref DLDDFSInternal  <!-- Note link -->
   - @ref DLD-lspec-state
   - @ref DLD-lspec-thread
   - @ref DLD-lspec-numa


   @subsection DLD-lspec-comps Component Overview
   <i>Mandatory.
   This section describes the internal logical decomposition.
   A diagram of the interaction between internal components and
   between external consumers and the internal components is useful.</i>

   Doxygen is limited in its internal support for diagrams. It has built in
   support for @c dot and @c mscgen, and examples of both are provided in this
   template.  Please remember that every diagram @a must be accompanied by
   an explanation.

   The following @@dot diagram shows the internal components of the Network
   layer, and also illustrates its primary consumer, the RPC layer.
   @dot
   digraph {
     node [style=box];
     label = "Network Layer Components and Interactions";
     subgraph cluster_rpc {
         label = "RPC Layer";
         rpcC [label="Connectivity"];
	 rpcO [label="Output"];
     }
     subgraph cluster_net {
         label = "Network Layer";
	 netM [label="Messaging"];
	 netT [label="Transport"];
	 netL [label="Legacy RPC emulation", style="filled"];
	 netM -> netT;
	 netL -> netM;
     }
     rpcC -> netM;
     rpcO -> netM;
   }
   @enddot

   The @@msc command is used to invoke @c mscgen, which creates sequence
   diagrams. For example:
   @msc
   a,b,c;

   a->b [ label = "ab()" ] ;
   b->c [ label = "bc(TRUE)"];
   c=>c [ label = "process(1)" ];
   c=>c [ label = "process(2)" ];
   ...;
   c=>c [ label = "process(n)" ];
   c=>c [ label = "process(END)" ];
   a<<=c [ label = "callback()"];
   ---  [ label = "If more to run", ID="*" ];
   a->a [ label = "next()"];
   a->c [ label = "ac1()\nac2()"];
   b<-c [ label = "cb(TRUE)"];
   b->b [ label = "stalled(...)"];
   a<-b [ label = "ab() = FALSE"];
   @endmsc
   Note that when entering commands for @c mscgen, do not include the
   <tt>msc { ... }</tt> block delimiters.
   You need the @c mscgen program installed on your system - it is part
   of the Scientific Linux based DevVM.

   UML and sequence diagrams often illustrate points better than any written
   explanation.  However, you have to resort to an external tool to generate
   the diagram, save the image in a file, and load it into your DLD.

   An image is relatively easy to load provided you remember that the
   Doxygen output is viewed from the @c doc/html directory, so all paths
   should be relative to that frame of reference.  For example:
   <img src="../../doc/dld/dld-sample-uml.png">
   I found that a PNG format image from Visio shows up with the correct
   image size while a GIF image was wrongly sized.  Your experience may
   be different, so please ensure that you validate the Doxygen output
   for correct image rendering.

   If an external tool, such as Visio or @c dia, is used to create an
   image, the source of that image (e.g. the Visio <tt>.vsd</tt> or
   the <tt>.dia</tt> file) should be checked into the source tree so
   that future maintainers can modify the figure.  This applies to all
   non-embedded image source files, not just Visio or @c dia.

   @subsection DLD-lspec-sc1 Subcomponent design
   <i>Such sections briefly describes the purpose and design of each
   sub-component. Feel free to add multiple such sections, and any additional
   sub-sectioning within.</i>

   Sample non-standard sub-section to illustrate that it is possible to
   document the design of a sub-component.  This contrived example demonstrates
   @@subsubsections for the sub-component's data structures and subroutines.

   @subsubsection DLD-lspec-ds1 Subcomponent Data Structures
   <i>This section briefly describes the internal data structures that are
   significant to the design of the sub-component. These should not be a part
   of the Functional Specification.</i>

   Describe @a briefly the internal data structures that are significant to
   the design.  These should not be described in the Functional Specification
   as they are not part of the external interface.  It is <b>not necessary</b>
   to describe all the internal data structures here.  They should, however, be
   documented in Detailed Functional Specifications, though separate from the
   external interfaces.  See @ref DLDDFSInternal for example.

   - dld_sample_internal

   @subsubsection DLD-lspec-sub1 Subcomponent Subroutines
   <i>This section briefly describes the interfaces of the sub-component that
   are of significance to the design.</i>

   Describe @a briefly the internal subroutines that are significant to the
   design.  These should not be described in the Functional Specification as
   they are not part of the external interface.  It is <b>not necessary</b> to
   describe all the internal subroutines here.  They should, however, be
   documented in Detailed Functional Specifications, though separate from the
   external interfaces.  See @ref DLDDFSInternal for example.

   - dld_sample_internal_invariant()

   @subsection DLD-lspec-state State Specification
   <i>Mandatory.
   This section describes any formal state models used by the component,
   whether externally exposed or purely internal.</i>

   Diagrams are almost essential here. The @@dot tool is the easiest way to
   create state diagrams, and is very readable in text form too.  Here, for
   example, is a @@dot version of a figure from the "rpc/session.h" file:
   @dot
   digraph example {
       size = "5,6"
       label = "RPC Session States"
       node [shape=record, fontname=Helvetica, fontsize=10]
       S0 [label="", shape="plaintext", layer=""]
       S1 [label="Uninitialized"]
       S2 [label="Initialized"]
       S3 [label="Connecting"]
       S4 [label="Active"]
       S5 [label="Terminating"]
       S6 [label="Terminated"]
       S7 [label="Uninitialized"]
       S8 [label="Failed"]
       S0 -> S1 [label="allocate"]
       S1 -> S2 [label="m0_rpc_conn_init()"]
       S2 -> S3 [label="m0_rpc_conn_established()"]
       S3 -> S4 [label="m0_rpc_conn_establish_reply_received()"]
       S4 -> S5 [label="m0_rpc_conn_terminate()"]
       S5 -> S6 [label="m0_rpc_conn_terminate_reply_received()"]
       S6 -> S7 [label="m0_rpc_conn_fini()"]
       S2 -> S8 [label="failed"]
       S3 -> S8 [label="timeout or failed"]
       S5 -> S8 [label="timeout or failed"]
       S8 -> S7 [label="m0_rpc_conn_fini()"]
   }
   @enddot
   The @c dot program is part of the Scientific Linux DevVM.

   @subsection DLD-lspec-thread Threading and Concurrency Model
   <i>Mandatory.
   This section describes the threading and concurrency model.
   It describes the various asynchronous threads of operation, identifies
   the critical sections and synchronization primitives used
   (such as semaphores, locks, mutexes and condition variables).</i>

   This section must explain all aspects of synchronization, including locking
   order protocols, existential protection of objects by their state, etc.
   A diagram illustrating lock scope would be very useful here.
   For example, here is a @@dot illustration of the scope and locking order
   of the mutexes in the Networking Layer:
   @dot
   digraph {
      node [shape=plaintext];
      subgraph cluster_m1 { // represents mutex scope
         // sorted R-L so put mutex name last to align on the left
         rank = same;
	 n1_2 [label="dom_fini()"];  // procedure using mutex
	 n1_1 [label="dom_init()"];
         n1_0 [label="m0_net_mutex"];// mutex name
      }
      subgraph cluster_m2 {
         rank = same;
	 n2_2 [label="tm_fini()"];
         n2_1 [label="tm_init()"];
         n2_4 [label="buf_deregister()"];
	 n2_3 [label="buf_register()"];
         n2_0 [label="nd_mutex"];
      }
      subgraph cluster_m3 {
         rank = same;
	 n3_2 [label="tm_stop()"];
         n3_1 [label="tm_start()"];
	 n3_6 [label="ep_put()"];
	 n3_5 [label="ep_create()"];
	 n3_4 [label="buf_del()"];
	 n3_3 [label="buf_add()"];
         n3_0 [label="ntm_mutex"];
      }
      label="Mutex usage and locking order in the Network Layer";
      n1_0 -> n2_0;  // locking order
      n2_0 -> n3_0;
   }
   @enddot

   @subsection DLD-lspec-numa NUMA optimizations
   <i>Mandatory for components with programmatic interfaces.
   This section describes if optimal behavior can be supported by
   associating the utilizing thread to a single processor.</i>

   Conversely, it can describe if sub-optimal behavior arises due
   to contention for shared component resources by multiple processors.

   The section is marked mandatory because it forces the designer to
   consider these aspects of concurrency.

   <hr>
   @section DLD-conformance Conformance
   <i>Mandatory.
   This section cites each requirement in the @ref DLD-req section,
   and explains briefly how the DLD meets the requirement.</i>

   Note the subtle difference in that @b I tags are used instead of
   the @b R  tags of the requirements section.  The @b I of course,
   stands for "implements":

   - @b I.DLD.Structured The DLD specification provides a structural
   breakdown along the lines of the HLD specification.  This makes it
   easy to understand and analyze the various facets of the design.
   - @b I.DLD.What The DLD style guide requires that a
   DLD contain a Functional Specification section.
   - @b I.DLD.How The DLD style guide requires that a
   DLD contain a Logical Specification section.
   - @b I.DLD.Maintainable The DLD style guide requires that the
   DLD be written in the main header file of the component.
   It can be maintained along with the code, without
   requiring one to resort to other documents and tools.  The only
   exception to this would be for images referenced by the DLD specification,
   as Doxygen does not provide sufficient support for this purpose.

   This section is meant as a cross check for the DLD writer to ensure
   that all requirements have been addressed.  It is recommended that you
   fill it in as part of the DLD review.

   <hr>
   @section DLD-ut Unit Tests
   <i>Mandatory. This section describes the unit tests that will be designed.
   </i>

   Unit tests should be planned for all interfaces exposed by the
   component.  Testing should not just include correctness tests, but
   should also test failure situations.  This includes testing of
   @a expected return error codes when presented with invalid
   input or when encountering unexpected data or state.  Note that
   assertions are not testable - the unit test program terminates!

   Another area of focus is boundary value tests, where variable
   values are equal to but do not exceed their maximum or minimum
   possible values.

   As a further refinement and a plug for Test Driven Development, it
   would be nice if the designer can plan the order of development of
   the interfaces and their corresponding unit tests.  Code inspection
   could overlap development in such a model.

   Testing should relate to specific use cases described in the HLD if
   possible.

   It is acceptable that this section be located in a separate @@subpage like
   along the lines of the Functional Specification.  This can be deferred
   to the UT phase where additional details on the unit tests are available.

   Use the Doxygen @@test tag to identify each test.  Doxygen collects these
   and displays them on a "Test List" page.

   <hr>
   @section DLD-st System Tests
   <i>Mandatory.
   This section describes the system testing done, if applicable.</i>

   Testing should relate to specific use cases described in the HLD if
   possible.

   It is acceptable that this section be located in a separate @@subpage like
   along the lines of the Functional Specification.  This can be deferred
   to the ST phase where additional details on the system tests are available.


   <hr>
   @section DLD-O Analysis
   <i>This section estimates the performance of the component, in terms of
   resource (memory, processor, locks, messages, etc.) consumption,
   ideally described in big-O notation.</i>

   <hr>
   @section DLD-ref References
   <i>Mandatory. Provide references to other documents and components that
   are cited or used in the design.
   In particular a link to the HLD for the DLD should be provided.</i>

   - <a href="https://docs.google.com/a/xyratex.com/
Doc?docid=0ATg1HFjUZcaZZGNkNXg4cXpfMjQ3Z3NraDI4ZG0&hl=en_US">
Detailed level design HOWTO</a>,
   an older document on which this style guide is partially based.
   - <a href="http://www.stack.nl/~dimitri/doxygen/manual.html">Doxygen
   Manual</a>
   - <a href="http://www.graphviz.org">Graphviz - Graph Visualization
   Software</a> for documentation on the @c dot command.
   - <a href="http://www.mcternan.me.uk/mscgen">Mscgen home page</a>

   <hr>
   @section DLD-impl-plan Implementation Plan
   <i>Mandatory.  Describe the steps that should be taken to implement this
   design.</i>

   The plan should take into account:
   - The need for early exposure of new interfaces to potential consumers.
   Should parts of the interfaces be landed early with stub support?
   Does consumer code have to be updated with such stubs?
   - Modular and functional decomposition.  Identify pieces that can be
   developed (coded and unit tested) in smaller sub-tasks.  Smaller tasks
   demonstrate progress to management, as well as reduce the inspection
   overhead.  It may be necessary to plan on re-factoring pieces of code
   to support this incremental development approach.
   - Understand how long it will take to implement the design. If it is
   significantly long (more than a few weeks), then task decomposition
   becomes even more essential, because it allows for merging of changes
   from master into your feature branch, if necessary.
   Task decomposition should be reflected in the GSP task plan for the sprint.
   - Determine how to maximize the overlap of inspection and ongoing
   development.
   The smaller the inspection task the faster it could complete.
   Good planning can reduce programmer idle time during inspection;
   being able to overlap development of the next coding sub-task while the
   current one is being inspected is ideal!
   It is useful to anticipate how you would need organize your GIT code
   source branches to handle this efficiently.
   Remember that you should only present modified code for inspection,
   and not the changes you picked up with periodic merges from another branch.
   - The software development process used by Mero provides sufficient
   flexibility to decompose tasks, consolidate phases, etc.  For example,
   you may prefer to develop code and UT together, and present both for
   inspection at the same time.  This would require consolidation of the
   CINSP-PREUT phase into the CINSP-POSTUT phase.
   Involve your inspector in such decisions.
   Document such changes in this plan, update the task spreadsheet for both
   yourself and your inspector.

   The implementation plan should be deleted from the DLD when the feature
   is landed into master.

 */


#include "doc/dld/dld_template.h"

/**
   @defgroup DLDDFSInternal Mero Sample Module Internals
   @brief Detailed functional specification of the internals of the
   sample module.

   This example is part of the DLD Template and Style Guide. It illustrates
   how to keep internal documentation separate from external documentation
   by using multiple @@defgroup commands in different files.

   Please make sure that the module cross-reference the DLD, as shown below.

   @see @ref DLD and @ref DLD-lspec

   @{
 */

/** Structure used internally */
struct dld_sample_internal {
	int dsi_f1; /**< field to do blah */
};

/** Invariant for dld_sample_internal must be called holding the mutex */
static bool dld_sample_internal_invariant(const struct dld_sample_internal *dsi)
{
	if (dsi->dsi-f1 == 0)
		return false;
	return true;
}

/** @} */ /* end internal */

/**
   External documentation can be continued if need be - usually it should
   be fully documented in the header only.
   @addtogroup DLDFS
   @{
 */

/**
 * This is an example of bad documentation, where an external symbol is
 * not documented in the externally visible header in which it is declared.
 * This also results in Doxygen not being able to automatically reference
 * it in the Functional Specification.
 */
unsigned int dld_bad_example;

/** @} */ /* end-of-DLDFS */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 79
 *  scroll-step: 1
 *  End:
 */
