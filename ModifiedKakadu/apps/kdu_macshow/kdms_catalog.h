/*****************************************************************************/
// File: kdms_catalog.h [scope = APPS/MACSHOW]
// Version: Kakadu, V6.1
// Author: David Taubman
// Last Revised: 28 November, 2008
/*****************************************************************************/
// Copyright 2001, David Taubman, The University of New South Wales (UNSW)
// The copyright owner is Unisearch Ltd, Australia (commercial arm of UNSW)
// Neither this copyright statement, nor the licensing details below
// may be removed from this file or dissociated from its contents.
/*****************************************************************************/
// Licensee: University of Arizona
// License number: 00307
// The licensee has been granted a UNIVERSITY LIBRARY license to the
// contents of this source file.  A brief summary of this license appears
// below.  This summary is not to be relied upon in preference to the full
// text of the license agreement, accepted at purchase of the license.
// 1. The License is for University libraries which already own a copy of
//    the book, "JPEG2000: Image compression fundamentals, standards and
//    practice," (Taubman and Marcellin) published by Kluwer Academic
//    Publishers.
// 2. The Licensee has the right to distribute copies of the Kakadu software
//    to currently enrolled students and employed staff members of the
//    University, subject to their agreement not to further distribute the
//    software or make it available to unlicensed parties.
// 3. Subject to Clause 2, the enrolled students and employed staff members
//    of the University have the right to install and use the Kakadu software
//    and to develop Applications for their own use, in their capacity as
//    students or staff members of the University.  This right continues
//    only for the duration of enrollment or employment of the students or
//    staff members, as appropriate.
// 4. The enrolled students and employed staff members of the University have the
//    right to Deploy Applications built using the Kakadu software, provided
//    that such Deployment does not result in any direct or indirect financial
//    return to the students and staff members, the Licensee or any other
//    Third Party which further supplies or otherwise uses such Applications.
// 5. The Licensee, its students and staff members have the right to distribute
//    Reusable Code (including source code and dynamically or statically linked
//    libraries) to a Third Party, provided the Third Party possesses a license
//    to use the Kakadu software, and provided such distribution does not
//    result in any direct or indirect financial return to the Licensee,
//    students or staff members.  This right continues only for the
//    duration of enrollment or employment of the students or staff members,
//    as appropriate.
/******************************************************************************
 Description:
   Defines the `kdms_catalog' class, which provides a NSOutlineViewDataSource
 object for the metadata catalog outline view available for JPX files.  The
 object keeps track of labels as they become available, as well as changes
 in the label metadata brought about by editing operations.  It builds
 organized collections of labels, which can be used to navigate to associated
 image entities and/or image regions.
 *****************************************************************************/
#import <Cocoa/Cocoa.h>
#include "kdu_compressed.h"
#include "jpx.h"

class kdms_renderer;

// Defined here:
@class kdms_catalog_item;
struct kdms_catalog_node;
@class kdms_catalog;

// Defined elsewhere:
class kdms_renderer;

#define KDMS_CATALOG_TYPE_INDEX 0
#define KDMS_CATALOG_TYPE_ENTITIES 1
#define KDMS_CATALOG_TYPE_REGIONS 2
#define KDMS_CATALOG_NUM_TYPES 3

#define KDMS_CATALOG_PREFIX_LEN 40

/*****************************************************************************/
/*                           kdms_catalog_item                               */
/*****************************************************************************/

@interface kdms_catalog_item : NSObject {
  kdms_catalog_node *node;
}
-(kdms_catalog_item *)initWithNode:(kdms_catalog_node *)node;
-(kdms_catalog_node *)get_node;

@end // kdms_catalog_item

/*****************************************************************************/
/*                           kdms_catalog_node                               */
/*****************************************************************************/

struct kdms_catalog_node {
public: // Member functions
  kdms_catalog_node(const char *label, int catalog_type);
    /* Creates a new node with the indicated label. */
  kdms_catalog_node(kdms_catalog_node *ref, int prefix_length);
    /* Used to create intermediate nodes (children of prefix nodes).  The
     new node gets its `catalog_type' from `ref' and its label becomes
     the first `prefix_length' characters from `ref->label_prefix',
     followed by "...". */
  ~kdms_catalog_node();
    /* Deletes the node and all its children.  Also clears the application
     state reference associated with any non-empty `metanode' interfaces. */
  bool change_label(const char *new_label);
    /* Call this to change the label associated with an existing node.  This
     may require the node to be unlinked and relinked into its parent to
     correct ensure it is correctly ordered.  The function returns true
     if the label actually did change; otherwise, the actual text content
     of the label was actually the same as before. */
  void insert(kdms_catalog_node *child);
    /* Inserts `child' as a descendant of the current node, which may be
     a root node, or any other node.  No checking is done to see if the
     child is appropriate, with respect to the JPX metanode hierarchy,
     but `catalog_type' is checked as a convenience.
     The function's main features are: 1) it inserts the
     child in alphabetical order, with respect to the upper-case converted
     unicode label representation stored in `label_prefix'; 2) it converts
     the current node to a "prefix" node if the number of children becomes
     too large (see notes on `child_prefix'); and 3) it recursively follows
     prefix branches in order to insert the new child at the correct
     location. */
  void unlink();
    /* Unlinks the node from its parent and siblings, leaving its
     sibling and `parent' pointers all equal to NULL.  It can
     subsequently be inserted into another node if you like. */
  void note_children_increased(NSOutlineView *outline_view);
    /* This function is called if an update operation is known to have
     caused the number of children associated with a node to increase.
     If the node is currently expanded, it should be collapsed and
     re-expanded.  If expandability of the node changes, it must be
     reloaded. */
  void note_children_decreased(NSOutlineView *outline_view);
    /* This function is called if an update operation is known to have
     caused the number of children associated with a node to decrease. 
     If the node is currently expanded, it should be collapsed and
     re-expanded.  If expandability of the node changes, it must be
     reloaded. */
  void note_children_changed(NSOutlineView *outline_view);
    /* This function is called if the order of children for the node has
     changed.  The only safe thing to do is to collapse and expand the
     node, if it is currently expanded. */
  void note_label_changed(NSOutlineView *outline_view);
    /* This function is called if the label associated with a node has
     changed, but this has not caused any reordering of the node amongst
     its siblings. */
  void reflect_changes_part1(NSOutlineView *outline_view);
    /* This function is used together with `reflect_changes_part2' to
     process the changes recorded by the above `note...' functions.  This
     allows the effects of many changes to be efficiently reflected in a
     much smaller number of operations.  The function recursively scans
     through the nodes which need attention, collapsing nodes which
     are marked with the `needs_collapse' flag, from the bottom of the
     tree upwards. */
  void reflect_changes_part2(NSOutlineView *outline_view);
    /* This function recursively scans through the nodes which need attention,
     expanding and reloading nodes on the way down, as required. */
private:
  int compare(kdms_catalog_node *ref, int num_chars=0);
    /* Compares the current and `ref' nodes via their `label_prefix' arrays,
     returning 0 if the strings are equal, -1 if the current node's label
     precedes `ref' and 1 otherwise.  If `num_chars' is greater than 0, only
     the first `num_chars' are actually compared. */
  void convert_to_prefix_node();
    /* Converts the current node to a prefix node, introducing intermediate
     nodes as required to make it so. */
public: // Data
  jpx_metanode metanode; // Can be empty -- see below
  kdms_catalog_item *item; // Required to integrate with NSOutlineView.
  int catalog_type; // One of the `KDMS_CATALOG_TYPE_...' constants
  union {
    NSString *label; // All nodes have a label
    NSAttributedString *attributed_label; // For rich strings
  };
  int label_length; // Number of characters in `label'
  unichar label_prefix[KDMS_CATALOG_PREFIX_LEN+1];
      // null-terminated uppercase version of the label, truncated to at most
      // `KDMS_CATLOG_PREFIX_LEN' characters.
  bool needs_collapse; // If this node needs to be collapsed
  bool needs_expand; // If this node needs to be expanded (collapse first)
  bool needs_reload; // If this node needs to be reloaded
  bool descendant_needs_attention; // If any descendant needs any of above
  int child_prefix; // If > 0, this is a prefix node (see notes below)
  int num_children;
  int num_reducible_children; // See notes below
  kdms_catalog_node *parent;
  kdms_catalog_node *children;
  kdms_catalog_node *next_sibling; // Doubly-linked list of siblings
  kdms_catalog_node *prev_sibling;
};
  /* Notes:
     For efficiency reasons, the initial prefix of up to
   KDMS_CATALOG_PREFIX_LEN characters of the label string are converted to
   upper case and stored in the `label_prefix' array when the object is
   constructed.  This is used for searching and other related operations,
   since direct operations on the NSString class may cause the allocation
   of a large amount of temporary memory that might not be released until
   the thread of control returns all the way back to the run loop.
     All nodes in the tree must have non-NULL `label' strings.  However,
   `metanode' can be empty under the following conditions:
     1) `metanode' is empty for root nodes; for these nodes, the `label'
   string provides a textual description of the `catalog_type' for which it
   is the root.
     2) If a node has a non-zero `child_prefix' member, we say that it is
   a "prefix" node.  Any child of a prefix node which contains a
   non-empty `metanode' member must have a label with at most `child_prefix'
   characters.  All other children (probably all of them) have empty
   `metanode' members and labels of the form "<prefix>...", where
   "<prefix>" is the `child_prefix'-character prefix (converted to upper
   case) of all of its descendants.  This mechanism allows us to split
   long lists of children into alphabetical collections.  The
   `insert_node' function takes care of managing this process.
     To facilitate the determination of when "prefix" nodes should be
   created, the `min_reducible_children' member keeps track of
   the number of the amount by which the number of children could be
   reduced by converting the present node into a "prefix" node.  This value
   is readily updated while inserting nodes. */

/*****************************************************************************/
/*                              kdms_catalog                                 */
/*****************************************************************************/


@interface kdms_catalog : NSObject {
  NSOutlineView *outline_view;
  kdms_renderer *renderer;
  kdms_catalog_node *root_nodes[KDMS_CATALOG_NUM_TYPES];
}
-(void)initWithOutlineView:(NSOutlineView *)view
                  renderer:(kdms_renderer *)renderer;
-(void)update:(jpx_meta_manager)meta_manager;
  /* Scans all newly available/changed/deleted metadata nodes and updates the
   internal structure accordingly.  When invoking this function for the
   first time, you should first call `jpx_metnode::touch' on the
   root node of the metadata tree (as returned by `meta_manager.access_root'),
   so as to ensure that all nodes in the metadata tree can be accessed via
   `meta_manager.get_touched_nodes'. */
-(void)dealloc;
-(jpx_metanode)get_selected_metanode;
  /* Returns the node which is currently selected, if any. */
-(void)select_matching_metanode:(jpx_metanode)metanode;
  /* Searches for the supplied metanode in the tree.  If it is found, the
   node's parent is expanded (if necessary) and the node is selected. */

// Methods used to implement the NSOutlineViewDataSource informal protocol
-(id)outlineView:(NSOutlineView *)outlineView
           child:(NSInteger)index
          ofItem:(id)item;
-(BOOL)outlineView:(NSOutlineView *)outlineView
       isItemExpandable:(id)item;
-(NSInteger)outlineView:(NSOutlineView *)outlineView
            numberOfChildrenOfItem:(id)item;
-(id)outlineView:(NSOutlineView *)view
     objectValueForTableColumn:(NSTableColumn *)tableColumn
          byItem:(id)item;

@end // kdms_catalog

