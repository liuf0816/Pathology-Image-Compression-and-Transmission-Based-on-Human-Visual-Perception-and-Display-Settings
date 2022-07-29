/*****************************************************************************/
// File: kdms_metadata_editor.h [scope = APPS/MACSHOW]
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
    Defines the `kdms_metadata_editor' Objective-C class, which implements a
dialog for adding and editing simple metadata boxes, which may be associated
with regions of interest.  This class goes hand in hand with the dialog
window and associated controls, defined in the "metadata_editor.nib" file.
******************************************************************************/

#import <Cocoa/Cocoa.h>
#include "kdms_renderer.h"

// Declared here:
struct kdms_matching_metanode;
class kdms_matching_metalist;
struct kdms_box_template;
class kdms_metanode_edit_state;
class kdms_metadata_dialog_state;
@class kdms_metadata_editor;

/*****************************************************************************/
/*                           kdms_matching_metanode                          */
/*****************************************************************************/

struct kdms_matching_metanode {
  jpx_metanode node;
  kdms_matching_metanode *next;
  kdms_matching_metanode *prev;
};

/*****************************************************************************/
/*                           kdms_matching_metalist                          */
/*****************************************************************************/

class kdms_matching_metalist {
  /* This class is used to build a list of top-level jpx_metanodes which
   match the current cursor point for the `kdms_renderer::menu_MetadataEdit'
   function.  A top-level matching jpx_metanode is one which is not a
   descendant of another matching node.  The list does not include any
   numlist nodes, since numlist nodes have no meaning in and of themselves,
   so it makes no sense to present them to an editor.  To facilitate this,
   the `append_node_expanding_numlists' function is provided. */
public: // Member functions
  kdms_matching_metalist() { head = tail = NULL; }
  ~kdms_matching_metalist()
    { while ((tail=head) != NULL) { head=tail->next; delete tail; } }
  kdms_matching_metanode *find_container(jpx_metanode node);
    /* Searches for an existing element in the list, which is either equal
     to `node' or an acestor of `node'.  Returns NULL if none can be found. */
  kdms_matching_metanode *find_member(jpx_metanode node);
    /* Searches for an existing element in the list, which is either equal
     to `node' or a descendant of `node'.  Returns NULL if none can be
     found. */
  void delete_item(kdms_matching_metanode *entry);
    /* Deletes `entry' from the list managed by the object. */
  kdms_matching_metanode *append_node(jpx_metanode node);
    /* Creates a new `kdms_matching_metanode' item to hold `node' and
     appends it to the tail of the internal list, returning a pointer to
     the created item.  This function does no check to see whether or not
     an item already exists on the list which represents `node' or one
     of its ancestors. */
  void append_node_expanding_numlists_and_free_boxes(jpx_metanode node);
    /* This function does the same as `append_node', unless `node'
     happens to be a numlist or a free box, in which case its immediate
     children are added to the list.  The function is recursive, to cater for
     the possibility that a numlist or free node contains a numlist or
     free child. */
  void append_image_wide_nodes(jpx_meta_manager manager,
                               int layer_idx, int stream_idx);
    /* This function traverses the collection of image-wide (i.e., not
     ROI-specific) metadata nodes which can be associated with a
     given compositing layer and/or codestream.  If `layer_idx' >= 0,
     metadata which is associated specifically with the compositing layer is
     examined first.  Then, if `stream_idx' >= 0, metadata associated with
     the indicated codestream is examined next.  Finally, metadata which is
     not associated with any specific compositing layer or codestream is 
     examined.  Following this scanning order, the function adds only
     top-level numlists (i.e., those which are not descendants of
     matching nodes), expanding matching numlist nodes, as explained in
     connection with the `append_node_expanding_numlists' function. */ 
  kdms_matching_metanode *get_head() { return head; }
private: // Data
  kdms_matching_metanode *head, *tail;
};

/*****************************************************************************/
/*                             kdms_box_template                             */
/*****************************************************************************/

struct kdms_box_template {
  kdu_uint32 box_type;
  char box_type_string[5]; // 4-character code in printable form
  const char *file_suffix; // Always a non-empty string
  const char *initializer; // NULL for binary external file representations
public: // Convenience initializer
  void init(kdu_uint32 box_type, const char *suffix, const char *initializer)
    {
      this->box_type=box_type; this->initializer=initializer;
      box_type_string[0] = make_4cc_char((char)(box_type>>24));
      box_type_string[1] = make_4cc_char((char)(box_type>>16));
      box_type_string[2] = make_4cc_char((char)(box_type>>8));
      box_type_string[3] = make_4cc_char((char)(box_type>>0));
      box_type_string[4] = '\0';
      this->file_suffix = (suffix==NULL)?box_type_string:suffix;
    }
private: // Convenience function
  char make_4cc_char(char ch)
    {
      if (ch == ' ')
        return '_';
      if ((ch < 0x20) || (ch & 0x80))
        return '.';
      return ch;
    }
};

// Indices into an array of templates:
#define KDMS_LABEL_TEMPLATE 0
#define KDMS_XML_TEMPLATE 1
#define KDMS_IPR_TEMPLATE 2
#define KDMS_CUSTOM_TEMPLATE 3 // Used if original box type none of above
#define KDMS_NUM_BOX_TEMPLATES (KDMS_CUSTOM_TEMPLATE+1)

/*****************************************************************************/
/*                          kdms_metanode_edit_state                         */
/*****************************************************************************/

class kdms_metanode_edit_state {
public: // Member functions
  kdms_metanode_edit_state(jpx_meta_manager manager,
                           kdms_file_services *file_server);
  void move_to_parent();
  void move_to_next();
  void move_to_prev();
  void move_to_descendants();
  void delete_cur_node(kdms_renderer *renderer);
    /* Deletes the `cur_node' and the JPX data source and makes all required
     adjustments in order to obtain new valid `cur_node', `root_node' and
     `edit_item' values, if possible.  If there is no relevant new node,
     the function returns with `cur_node' empty.  The `renderer' object
     is provided so that its `metadata_changed' function can be called once
     the node has been deleted. */
  void add_child_node(kdms_renderer *renderer);
    /* Adds a descendant to the current node and adjusts the `cur_node'
     member to reference the new child.  The `renderer' object is
     provided so that its `metadata_changed' function can be called once
     the node has been deleted. */
  void find_sibling_indices();
    /* Before calling this function, `cur_node' and `root_node' should exist
     and reference different metadata nodes, and the unknown sibling indices
     should be set to -ve values.  These are `cur_node_sibling_idx',
     `next_sibling_idx' and `prev_sibling_idx'.  The function uses the
     known values, if any, to help it find the unknown values. */
public: // Data
  jpx_meta_manager meta_manager; // Provided by the constructor
  kdms_matching_metalist *metalist; // Points to `internal_metalist' if not
            // opened to edit existing metadata.
  kdms_matching_metanode *edit_item; // Item from edit list; either this node
            // is being edited or else one of its descendants is being edited
            // NULL if we are adding metadata from scratch.
  jpx_metanode root_node; // Equals `edit_item->node' if editing; starts out
            // empty if adding metadata from scratch.
  jpx_metanode cur_node; // Either `root_node' or one of its descendants
  int cur_node_sibling_idx; // Which child `cur_node' is of its parent
  int next_sibling_idx; // Idx of the next non-numlist child of our parent
  int prev_sibling_idx; // Idx of the previous non-numlist child of our parent
    // The above two members are used only if `cur_node' is not the root node.
    // A -ve value means there is no next (resp. prev) non-numlist sibling.
private: // Private data
  kdms_matching_metalist internal_metalist;
  kdms_file_services *file_server; // Provided by the constructor
};

/*****************************************************************************/
/*                        kdms_metadata_dialog_state                         */
/*****************************************************************************/

class kdms_metadata_dialog_state {
public: // Functions
  kdms_metadata_dialog_state(int num_codestreams, int num_layers,
                             kdms_box_template *available_types,
                             kdms_file_services *file_server);
  ~kdms_metadata_dialog_state();
  bool compare_associations(kdms_metadata_dialog_state *ref);
    /* Returns true if the image-entity or ROI associations of this
     object are the same as those of the `ref' object. */
  bool compare_contents(kdms_metadata_dialog_state *ref);
    /* Returns true if the box-type, label-string and any other node content
     attributes are the same as those of the `ref' object. */
  bool compare(kdms_metadata_dialog_state *ref)
    { return compare_associations(ref) && compare_contents(ref); }
  int add_selected_codestream(int idx);
    /* Augments the `selected_codestreams' list.  Returns -ve if `idx' was
     already in the list. Otherwise returns the entry in the
     `selected_codestream' array which contains the new codestream index. */
  int add_selected_layer(int idx);
    /* Augments the `selected_layers' list.  Returns -ve if `idx' was already
     in the list.  Otherwise returns the entry in the `selected_layers'
     array which contains the new compositing layer index. */
  void configure_with_metanode(jpx_metanode node);
    /* Sets up the internal member values based on the information in
     the supplied `node'. */
  void copy(kdms_metadata_dialog_state *src);
    /* Copy contents from `src'. */
  kdms_file *get_external_file() { return external_file; }
  void set_external_file(kdms_file *file, bool new_file_just_retained=false)
    { // Manages the release/retain calls required to ensure that external
      // file resources will be properly cleaned up when not required.  The
      // `new_file_just_retained' argument should be true only if the file
      // has just been obtained using `kdms_file_services::retain...'.
      if ((this->external_file == file) && !new_file_just_retained) return;
      if (external_file != NULL) external_file->release();
      external_file = file;
      if ((file != NULL) && !new_file_just_retained)
        file->retain();
    }
  void set_external_file_replaces_contents()
    { if (external_file != NULL) external_file_replaces_contents = true; }
  bool save_to_external_file(kdms_metanode_edit_state *edit);
    /* Creates a temporary external file if necessary.  Saves the contents
     of the label string, or `edit->cur_node' to the file.  Returns false
     if nothing could be saved for some reason (e.g., the file was locked
     or there was nothing to save because a current node does not exist or
     already has an associated file. */
  bool save_metanode(kdms_metanode_edit_state *edit_state,
                     kdms_metadata_dialog_state *initial_state,
                     kdms_renderer *renderer);
    /* Uses the information contained in the object's members to create
     a new metadata node.  The new metanode becomes the new value of
     `edit_state->cur_node' and, if appropriate, `edit_state->root_node'.
        If `edit_state->cur_node' exists on entry, that node changed to
     the new node; this may require deletion of the existing node or moving
     it to a different parent.
        The `initial_state' object is compared against the current object
     to determine whether or not there are changes that need to be saved.
        If the numlist or ROI associations have changed, the
     `jpx_meta_manager::insert_node' function is used to create a new parent
     to hold the node which is being created (or to create the new node itself
     if it is an ROI description box node).  When this happens, the new node
     (or its parent) is appended to the edit list as a new top-level matching
     metanode and the `edit_state' members are all adjusted to reflect the
     new structure.
        The function returns false if the process could not be performed
     because some inappropriate condition was detected (and flagged to the
     user).  In this case, no changes are made.  Otherwise, the function
     returns true, even if there are no changes to save.
        The `renderer' object's `metadata_changed' function is invoked
     if any changes are made in the metadata structure.  If metadata is
     added only, the function is invoked with its `new_data_only'
     argument equal to true. */
public: // Data members describing associations for the node
  int num_codestreams; // Provided by the constructor
  int num_layers; // Provided by the constructor
  int num_selected_codestreams;
  int *selected_codestreams; // Indices of selected codestreams
  int num_selected_layers;
  int *selected_layers; // Indices of selected compositing layers
  bool rendered_result;
  bool whole_image;
  int num_roi_regions;       // Number of regions, in case of complex ROI
  kdu_dims roi_bounding_box; // Bounding box for all regions
  bool roi_is_elliptical;    // True if one or more regions are elliptical
  bool roi_is_encoded;       // True if one or more regions are encoded
public: // Data members describing box contents for the node
  kdms_box_template *available_types; // Array provided by the constructor
  kdms_box_template *offered_types[KDMS_NUM_BOX_TEMPLATES]; // Array of
                             // pointers into the `available_types' array
  int num_offered_types; // Number of options offered to the user
  kdms_box_template *selected_type; // Matches cur selection from offered list
  const char *label_string; // NULL if node holds an internally stored label.
         // Note taht this member and `external_file' can both be non-NULL if
         // the external file is just a copy of the label string; in this case,
         // `external_file_replaces_contents' will be false of course.
private: // We keep the following member private, since we need to be sure
         // to manage the retain/release calls correctly
  kdms_file *external_file; // If this reference is non-NULL, the present
         // object has retained it.  It may be obtained from an existing
         // metanode, in which case the same referece will be found in
         // `kdms_metanode_edit_state'.  Alternatively, it may have been
         // freshly created to hold user edits.
  bool external_file_replaces_contents; // True if `external_file'
         // is external file is anything other than a copy of the existing
         // box contents (i.e., if edited, or if file is provided by user).
  kdms_file_services *file_server; // Provided by the constructor
};

/*****************************************************************************/
/*                           kdms_metadata_editor                            */
/*****************************************************************************/

@interface kdms_metadata_editor : NSObject {
  IBOutlet NSWindow *dialog_window;
  IBOutlet NSTextField *x_pos_field;
  IBOutlet NSTextField *y_pos_field;
  IBOutlet NSTextField *width_field;
  IBOutlet NSTextField *height_field;
  IBOutlet NSMatrix *roi_shape_matrix;
  IBOutlet NSButton *roi_encoded_button;
  IBOutlet NSButton *roi_multiregion_button;
  IBOutlet NSButton *whole_image_button;
  
  IBOutlet NSPopUpButton *codestream_popup;
  IBOutlet NSTextField *codestream_field;
  IBOutlet NSButton *codestream_add_button;
  IBOutlet NSButton *codestream_remove_button;
  IBOutlet NSStepper *codestream_stepper;
  IBOutlet NSPopUpButton *compositing_layer_popup;
  IBOutlet NSTextField *compositing_layer_field;
  IBOutlet NSButton *compositing_layer_add_button;
  IBOutlet NSButton *compositing_layer_remove_button;
  IBOutlet NSStepper *compositing_layer_stepper;
  IBOutlet NSButton *rendered_result_button;
  
  IBOutlet NSPopUpButton *box_type_popup;
  IBOutlet NSPopUpButton *box_editor_popup;
  IBOutlet NSTextField *label_field;
  IBOutlet NSTextField *external_file_field;
  IBOutlet NSButton *temporary_file_button;
  IBOutlet NSButton *save_to_file_button;
  IBOutlet NSButton *edit_file_button;
  IBOutlet NSButton *choose_file_button;

  IBOutlet NSButton *apply_button;
  IBOutlet NSButton *apply_and_exit_button;
  IBOutlet NSButton *delete_button;
  IBOutlet NSButton *exit_button;
  IBOutlet NSButton *next_button;
  IBOutlet NSTextField *next_button_label;
  IBOutlet NSButton *prev_button;
  IBOutlet NSTextField *prev_button_label;
  IBOutlet NSButton *parent_button;
  IBOutlet NSTextField *parent_button_label;
  IBOutlet NSButton *descendants_button;
  IBOutlet NSTextField *descendants_button_label;
  IBOutlet NSButton *add_child_button;
  IBOutlet NSButton *metashow_button;
  IBOutlet NSButton *catalog_button;

  bool allow_edits; // If editing is allowed
  int num_codestreams;
  int num_layers;
  kdms_box_template available_types[KDMS_NUM_BOX_TEMPLATES];
  kdms_file_services *file_server;
  kdms_metanode_edit_state *edit_state; // For C++ objs which need constructor
  kdms_metadata_dialog_state *state;
  kdms_metadata_dialog_state *initial_state;
       // Provision of two copies of the state allows one to keep track of
       // the original node parameters so we can see what has changed.
  kdms_renderer *renderer; // Non-NULL only within `run_modal' call.
  bool changed_something;
}

// Actions which are used to alter box associations
- (IBAction) removeCodestream:(id)sender;
- (IBAction) addCodestream:(id)sender;
- (IBAction) stepCodestream:(id)sender;
- (IBAction) showCodestream:(id)sender;
- (IBAction) removeCompositingLayer:(id)sender;
- (IBAction) addCompositingLayer:(id)sender;
- (IBAction) stepCompositingLayer:(id)sender;
- (IBAction) showCompositingLayer:(id)sender;
- (IBAction) setWholeImage:(id)sender;
- (IBAction) setSingleRegion:(id)sender;
- (IBAction) clearROIEncoded:(id)sender;

// Actions which are used to alter box contents
- (IBAction) changeBoxType:(id)sender; // Send when user clicks box type popup
- (IBAction) saveToFile:(id)sender;
- (IBAction) chooseFile:(id)sender;
- (IBAction) editFile:(id)sender;
- (IBAction) changeEditor:(id)sender;

// Actions which are used to navigate and control the editing session
- (IBAction) clickedExit:(id)sender;
- (IBAction) clickedDelete:(id)sender;
- (IBAction) clickedApply:(id)sender;
- (IBAction) clickedApplyAndExit:(id)sender;
- (IBAction) clickedNext:(id)sender;
- (IBAction) clickedPrev:(id)sender;
- (IBAction) clickedParent:(id)sender;
- (IBAction) clickedDescendants:(id)sender;
- (IBAction) clickedAddChild:(id)sender;
- (IBAction) findInMetashow:(id)sender;
- (IBAction) findInCatalog:(id)sender;

// Generic actions
- (IBAction) revertToInitialState:(id)sender;
- (IBAction) otherAction:(id)sender; // This message is sent by controls which
          // don't have any other actions, to give the present object an
          // opportunity to see if the action requires any update in the
          // enable state of other controls.  It invokes
          // `map_controls_to_state' followed by `update_apply_buttons'.

// Messages used to initialize, configure and run the editing dialog
- (void) initWithNumCodestreams:(int)codestreams
                         layers:(int)layers
                    metaManager:(jpx_meta_manager)manager
                   fileServices:(kdms_file_services *)file_services
                       editable:(bool)can_edit;
- (void) configureWithEditList:(kdms_matching_metalist *)metalist;
                  // To use this function, you should be sure that
                  // `metalist->get_head()' returns a non-empty list of
                  // matching metadata nodes.
- (void) configureWithInitialRegion:(kdu_dims)roi
                             stream:(int)codestream_idx
                              layer:(int)layer_idx
            appliesToRenderedResult:(bool)associate_with_rendered_result;

- (void) dealloc;
- (bool) run_modal:(kdms_renderer *)renderer;
                   // Returns true if any metadata was added or changed.
                   // Invokes `renderer->metadata_changed' whenever metadata
                   // is changed, so that the main view window will continue
                   // to display regions of interest correctly and the
                   // metashow window will be altered as required.
// Internal utility functions
- (void) map_state_to_controls; // Copies `state' to dialog controls
- (void) map_controls_to_state; // Copies dialog control info to `state'
- (void) update_apply_buttons; // Enables/disables the "Apply ..." buttons,
                      // based on whether or not there is anything to apply.

// Delegate functions
- (BOOL) control:(NSControl *)control isValidObject:(id)object;
         // Used by text-fields, which set this object as their delegate,
         // to reject non-integer values.

@end // kdms_metadata_editor
