/*****************************************************************************/
// File: kdms_catalog.mm [scope = APPS/MACSHOW]
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
 Implements the `kdms_catalog' Objective-C class.
 ******************************************************************************/
#import "kdms_metadata_editor.h"
#import "kdms_window.h"
#import "kdms_catalog.h"

/*===========================================================================*/
/*                              kdms_catalog_item                            */
/*===========================================================================*/

@implementation kdms_catalog_item

/*****************************************************************************/
/*                      kdms_catalog_item:initWithNode                       */
/*****************************************************************************/

-(kdms_catalog_item *)initWithNode:(kdms_catalog_node *)nd
{
  self->node = nd;
  return self;
}

/*****************************************************************************/
/*                        kdms_catalog_item:get_node                         */
/*****************************************************************************/

-(kdms_catalog_node *)get_node
{
  return node;
}

@end // kdms_catalog_item

/*===========================================================================*/
/*                              kdms_catalog_node                            */
/*===========================================================================*/

/*****************************************************************************/
/*                    kdms_catalog_node::kdms_catalog_node                   */
/*****************************************************************************/

kdms_catalog_node::kdms_catalog_node(const char *lbl, int catalog_type)
{
  item = nil;
  this->catalog_type = catalog_type;
  child_prefix = 0;
  num_reducible_children = 0;
  num_children = 0;
  parent = NULL;
  children = NULL;
  next_sibling = prev_sibling = NULL;
  label = [[NSString alloc] initWithUTF8String:lbl];
  label_length = (int) [label length];
  if (label_length > KDMS_CATALOG_PREFIX_LEN)
    {
      [[[label substringToIndex:KDMS_CATALOG_PREFIX_LEN]
        uppercaseString] getCharacters:label_prefix];
      label_prefix[KDMS_CATALOG_PREFIX_LEN] = 0;
    }
  else
    {
      [[label uppercaseString] getCharacters:label_prefix];
      label_prefix[label_length] = 0;
    }
  item = [[kdms_catalog_item alloc] initWithNode:this];
  needs_expand = needs_collapse = needs_reload = false;
  descendant_needs_attention = false;
}

/*****************************************************************************/
/*           kdms_catalog_node::kdms_catalog_node (prefix child)             */
/*****************************************************************************/

kdms_catalog_node::kdms_catalog_node(kdms_catalog_node *ref, int prefix_len)
{
  item = nil;
  this->catalog_type = ref->catalog_type;
  child_prefix = 0;
  num_reducible_children = 0;
  num_children = 0;
  parent = NULL;
  children = NULL;
  next_sibling = prev_sibling = NULL;
  
  // None of the following two conditions should occur
  if (prefix_len > ref->label_length)
    { assert(0); prefix_len = ref->label_length; }
  if (prefix_len > KDMS_CATALOG_PREFIX_LEN)
    { assert(0); prefix_len = KDMS_CATALOG_PREFIX_LEN; }
  
  int c;
  for (c=0; c < prefix_len; c++)
    label_prefix[c] = ref->label_prefix[c];
  for (; (c < KDMS_CATALOG_PREFIX_LEN) && (c < (prefix_len+3)); c++)
    label_prefix[c] = (unichar) '.';
  label_prefix[c] = 0;
  label = [[NSString alloc] initWithCharacters:label_prefix length:c];
  item = [[kdms_catalog_item alloc] initWithNode:this];
  needs_expand = needs_collapse = needs_reload = false;
  descendant_needs_attention = false;
}

/*****************************************************************************/
/*                    kdms_catalog_node::~kdms_catalog_node                  */
/*****************************************************************************/

kdms_catalog_node::~kdms_catalog_node()
{
  if (metanode.exists())
    metanode.set_state_ref(NULL);
  if (label != nil)
    { [label release]; label = nil; }
  if (item != nil)
    { [item release]; item = nil; }
  kdms_catalog_node *child;
  while ((child=children) != NULL)
    {
      children = child->next_sibling;
      delete child;
    }
}

/*****************************************************************************/
/*                       kdms_catalog_node::change_label                     */
/*****************************************************************************/

bool kdms_catalog_node::change_label(const char *new_lbl)
{
  NSString *new_label = [[NSString alloc] initWithUTF8String:new_lbl];
  int new_label_length = (int) [new_label length];
  if ((new_label_length == label_length) &&
      [new_label isEqualToString:label])
    {
      [new_label release];
      return false;
    }
  [label release];
  label = new_label;
  label_length = new_label_length;
  if (label_length > KDMS_CATALOG_PREFIX_LEN)
    {
      [[[label substringToIndex:KDMS_CATALOG_PREFIX_LEN]
        uppercaseString] getCharacters:label_prefix];
      label_prefix[KDMS_CATALOG_PREFIX_LEN] = 0;
    }
  else
    {
      [[label uppercaseString] getCharacters:label_prefix];
      label_prefix[label_length] = 0;
    }
  return true;
}

/*****************************************************************************/
/*                         kdms_catalog_node::insert                         */
/*****************************************************************************/

void kdms_catalog_node::insert(kdms_catalog_node *child)
{
  assert((child->parent == NULL) &&
         (child->next_sibling == NULL) && (child->prev_sibling == NULL));
  assert(catalog_type == child->catalog_type);
  
  kdms_catalog_node *scan, *prev;
  if (child->needs_expand || child->needs_collapse || child->needs_reload ||
      child->descendant_needs_attention)
    for (scan=this; (scan != NULL) && !scan->descendant_needs_attention;
         scan=scan->parent)
      scan->descendant_needs_attention = true;
         
  if ((child_prefix > 0) && (child->label_length > child_prefix))
    { // Find descendant into which we need to insert `child'
      for (prev=NULL, scan=children;
           scan != NULL; prev=scan, scan=scan->next_sibling)
        if (child->compare(scan,child_prefix) < 0)
          break; // Child node strictly precedes `scan' in alphabetical order
      kdms_catalog_node *elt = prev; // Potential parent for the child
      if ((elt == NULL) || (child->compare(elt,child_prefix) != 0))
        { // We have a new prefix to form
          elt = new kdms_catalog_node(child,child_prefix); // Creates a node
          elt->parent = this;
          if ((elt->prev_sibling = prev) == NULL)
            this->children = elt;
          else
            prev->next_sibling = elt;
          if ((elt->next_sibling = scan) != NULL)
            scan->prev_sibling = elt;
          this->num_children++;
        }
      elt->insert(child);
      return;
    }
  
  // If we get here, we are going to insert child as an immediate
  // descendant.
  for (prev=NULL, scan=children;
       scan != NULL; prev=scan, scan=scan->next_sibling)
    if (child->compare(scan) < 0)
      break; // We come strictly before `scan' in alphabetic order
  if ((child->prev_sibling = prev) == NULL)
    children = child;
  else
    prev->next_sibling = child;
  if ((child->next_sibling = scan) != NULL)
    scan->prev_sibling = child;
  child->parent = this;
  num_children++;
  
  // Update the `num_reducible_children' member
  int ref_length = 1;
  if (parent != NULL)
    ref_length = parent->child_prefix + 1;
  if ((ref_length >= (KDMS_CATALOG_PREFIX_LEN>>1)) ||
      (ref_length >= label_length))
    return; // Don't consider extremely deep hierarchies
  if ((prev != NULL) && (ref_length < prev->label_length) &&
      (prev->compare(child,ref_length) == 0))
    num_reducible_children++;
  else if ((scan != NULL) && (ref_length < scan->label_length) &&
           (scan->compare(child,ref_length) == 0))
    num_reducible_children++;
  
  if ((num_children >= 15) &&
      (num_reducible_children > (num_children >> 1)))
    convert_to_prefix_node();
}

/*****************************************************************************/
/*                         kdms_catalog_node::unlink                         */
/*****************************************************************************/

void kdms_catalog_node::unlink()
{
  if (parent == NULL)
    return; // Already unlinked
  if (prev_sibling == NULL)
    {
      assert(this == parent->children);
      parent->children = next_sibling;
    }
  else
    prev_sibling->next_sibling = next_sibling;
  if (next_sibling != NULL)
    next_sibling->prev_sibling = prev_sibling;
  parent->num_children--;
  if (parent->child_prefix == 0)
    { // See if we need to adjust `parent->num_reducible_children'
      int ref_length = 1;
      if (parent->parent != NULL)
        ref_length = parent->parent->child_prefix+1;
      if ((ref_length < (KDMS_CATALOG_PREFIX_LEN>>1)) &&
          (ref_length < label_length))
        {
          if ((prev_sibling != NULL) &&
              (ref_length < prev_sibling->label_length) &&
              (prev_sibling->compare(this,ref_length) == 0))
            parent->num_reducible_children--;
          else if ((next_sibling != NULL) &&
                   (ref_length < next_sibling->label_length) &&
                   (next_sibling->compare(this,ref_length) == 0))
            parent->num_reducible_children--;
        }
    }
  prev_sibling = next_sibling = parent = NULL;
}

/*****************************************************************************/
/*                 kdms_catalog_node::note_children_decreased                */
/*****************************************************************************/

void
  kdms_catalog_node::note_children_decreased(NSOutlineView *outline_view)
{
  bool needs_attention = false;
  if ([outline_view isItemExpanded:item])
    {
      needs_attention = needs_collapse = true;
      if (num_children > 0)
        needs_expand = true;
    }
  if ((num_children == 0) &&
      ((parent == NULL) || [outline_view isItemExpanded:parent->item]))
    needs_attention = needs_reload = true; // Expandability has changed
  if (needs_attention)
    for (kdms_catalog_node *scan=parent;
         (scan != NULL) && !scan->descendant_needs_attention;
         scan=scan->parent)
      scan->descendant_needs_attention = true;
}

/*****************************************************************************/
/*                 kdms_catalog_node::note_children_increased                */
/*****************************************************************************/

void
  kdms_catalog_node::note_children_increased(NSOutlineView *outline_view)
{
  if ([outline_view isItemExpanded:item])
    needs_collapse = needs_expand = true;
  else if ((num_children == 1) &&
           ((parent == NULL) || [outline_view isItemExpanded:parent->item]))
    needs_reload = true;
  else
    return;
  for (kdms_catalog_node *scan=parent;
       (scan != NULL) && !scan->descendant_needs_attention;
       scan=scan->parent)
    scan->descendant_needs_attention = true;
}

/*****************************************************************************/
/*                  kdms_catalog_node::note_children_changed                 */
/*****************************************************************************/

void
  kdms_catalog_node::note_children_changed(NSOutlineView *outline_view)
{
  if ([outline_view isItemExpanded:item])
    {
      needs_collapse = needs_expand = true;
      for (kdms_catalog_node *scan=parent;
           (scan != NULL) && !scan->descendant_needs_attention;
           scan=scan->parent)
        scan->descendant_needs_attention = true;
    }
}

/*****************************************************************************/
/*                   kdms_catalog_node::note_label_changed                   */
/*****************************************************************************/

void
  kdms_catalog_node::note_label_changed(NSOutlineView *outline_view)
{
  if ((parent == NULL) || [outline_view isItemExpanded:parent->item])
    {
      needs_reload = true;
      for (kdms_catalog_node *scan=parent;
           (scan != NULL) && !scan->descendant_needs_attention;
           scan=scan->parent)
        scan->descendant_needs_attention = true;
    }
}

/*****************************************************************************/
/*                 kdms_catalog_node::reflect_changes_part1                  */
/*****************************************************************************/

void
  kdms_catalog_node::reflect_changes_part1(NSOutlineView *outline_view)
{
  if (descendant_needs_attention)
    for (kdms_catalog_node *child=children; child != NULL;
         child=child->next_sibling)
      child->reflect_changes_part1(outline_view);
  if (needs_collapse)
    [outline_view collapseItem:item];
  needs_collapse = false;
}

/*****************************************************************************/
/*                 kdms_catalog_node::reflect_changes_part2                  */
/*****************************************************************************/

void
  kdms_catalog_node::reflect_changes_part2(NSOutlineView *outline_view)
{
  if (needs_reload)
    [outline_view reloadItem:item];
  if (needs_expand)
    [outline_view expandItem:item];
  needs_reload = needs_expand = false;
  if (descendant_needs_attention)
    for (kdms_catalog_node *child=children; child != NULL;
         child=child->next_sibling)
      child->reflect_changes_part2(outline_view);
  descendant_needs_attention = false;
}

/*****************************************************************************/
/*                        kdms_catalog_node::compare                         */
/*****************************************************************************/

int kdms_catalog_node::compare(kdms_catalog_node *ref, int num_chars)
{
  if ((num_chars <= 0) || (num_chars > KDMS_CATALOG_PREFIX_LEN))
    num_chars = KDMS_CATALOG_PREFIX_LEN;
  for (int n=0; n < num_chars; n++)
    if (label_prefix[n] < ref->label_prefix[n])
      return -1;
    else if (label_prefix[n] > ref->label_prefix[n])
      return 1;
    else if (label_prefix[n] == 0)
      break;
  return 0;
}

/*****************************************************************************/
/*                 kdms_catalog_node::convert_to_prefix_node                 */
/*****************************************************************************/

void kdms_catalog_node::convert_to_prefix_node()
{
  assert(child_prefix == 0);
  if (parent != NULL)
    child_prefix = parent->child_prefix+1;
  else
    child_prefix = 1;
  
  kdms_catalog_node *last_prefix = NULL;
  kdms_catalog_node *scan, *next;
  for (scan=children; scan != NULL; scan=next)
    {
      next = scan->next_sibling;
      if (scan->label_length <= child_prefix)
        continue; // Leave this one where it is
      if ((last_prefix == NULL) ||
          (scan->compare(last_prefix,child_prefix) != 0))
        { // We don't belong inside the last prefix, so let's make a new one
          last_prefix = new kdms_catalog_node(scan,child_prefix);
          last_prefix->parent = this;
          if ((last_prefix->prev_sibling = scan->prev_sibling) == NULL)
            this->children = last_prefix;
          else
            last_prefix->prev_sibling->next_sibling = last_prefix;
          last_prefix->next_sibling = scan;
          this->num_children++;
          scan->prev_sibling = last_prefix;
        }
      scan->unlink();
      last_prefix->insert(scan);
    }
  num_reducible_children = 0; // We are now a prefix node and don't need this
}


/*===========================================================================*/
/*                               kdms_catalog                                */
/*===========================================================================*/

@implementation kdms_catalog

/*****************************************************************************/
/*                kdms_catalog:initWithOutlineView:renderer                  */
/*****************************************************************************/

-(void)initWithOutlineView:(NSOutlineView *)view
                  renderer:(kdms_renderer *)the_renderer
{
  self->outline_view = view;
  self->renderer = the_renderer;
  for (int n=0; n < KDMS_CATALOG_NUM_TYPES; n++)
    root_nodes[n] = NULL;
  root_nodes[0] = new kdms_catalog_node("Index Labels",
                                        KDMS_CATALOG_TYPE_INDEX);
  root_nodes[1] = new kdms_catalog_node("Image-Entity Labels",
                                        KDMS_CATALOG_TYPE_ENTITIES);
  root_nodes[2] = new kdms_catalog_node("Region-Specific Labels",
                                        KDMS_CATALOG_TYPE_REGIONS);
  id objects[2], keys[2];
  keys[0] = NSForegroundColorAttributeName;
  keys[1] = NSFontAttributeName;
  objects[0] = [NSColor brownColor];
  CGFloat font_size = [NSFont
                       systemFontSizeForControlSize:NSRegularControlSize];
  objects[1] = [NSFont boldSystemFontOfSize:font_size];
  
  NSDictionary *root_attributes =
    [NSDictionary dictionaryWithObjects:objects forKeys:keys count:2];
  for (int n=0; n < KDMS_CATALOG_NUM_TYPES; n++)
    {
      kdms_catalog_node *root = root_nodes[n];
      if (root == NULL) continue;
      NSString *label = root->label;
      NSAttributedString *rich_label =
        [[NSAttributedString alloc] initWithString:label
                                        attributes:root_attributes];
      root->attributed_label = rich_label;
      [label release];
    }
  
  [outline_view setDataSource:self];
  [outline_view setTarget:self];
  [outline_view setDoubleAction:@selector(user_double_click:)];
}

/*****************************************************************************/
/*                          kdms_catalog:update                              */
/*****************************************************************************/

-(void)update:(jpx_meta_manager)meta_manager
{
  jpx_metanode selected_metanode = [self get_selected_metanode];
  jpx_metanode metanode;
  while ((metanode = meta_manager.get_touched_nodes()).exists())
    {
      const char *label = metanode.get_label();
      kdms_catalog_node *node = (kdms_catalog_node *) metanode.get_state_ref();
      assert((node == NULL) || (node->metanode == metanode));
      if (metanode.is_deleted() || (label == NULL))
        {
          if (node != NULL)
            {
              kdms_catalog_node *parent = node->parent;
              node->unlink();
              parent->note_children_decreased(outline_view);
              delete node;
            }
        }
      else
        {
          // Find the correct parent for the node
          kdms_catalog_node *new_parent = NULL;
          bool have_numlist=false, have_roi=false;
          jpx_metanode scan = metanode;
          while ((scan = scan.get_parent()).exists())
            {
              if ((new_parent =
                   (kdms_catalog_node *) scan.get_state_ref()) != NULL)
                {
                  assert(new_parent->metanode == scan);
                  break;
                }
              bool rres;
              int num_l, num_cs;
              if (scan.get_numlist_info(num_cs,num_l,rres) &&
                  ((num_cs > 0) || (num_l > 0) || rres))
                have_numlist = true;
              else if (scan.get_num_regions() > 0)
                have_roi = true;
            }
          if (new_parent == NULL)
            {
              if (have_roi)
                new_parent = root_nodes[KDMS_CATALOG_TYPE_REGIONS];
              else if (have_numlist)
                new_parent = root_nodes[KDMS_CATALOG_TYPE_ENTITIES];
              else
                new_parent = root_nodes[KDMS_CATALOG_TYPE_INDEX];
            }

          bool label_changed = false;
          if ((node != NULL) && metanode.is_changed())
            label_changed = node->change_label(label);
          
          if (node == NULL)
            {
              node = new kdms_catalog_node(label,new_parent->catalog_type);
              node->metanode = metanode;
              metanode.set_state_ref(node);
              new_parent->insert(node);
              new_parent->note_children_increased(outline_view);
            }
          else
            {
              kdms_catalog_node *old_parent = node->parent;
              kdms_catalog_node *old_prev = node->prev_sibling;
              kdms_catalog_node *old_next = node->next_sibling;
              node->unlink();
              node->catalog_type = new_parent->catalog_type;
              new_parent->insert(node);
              if (old_parent != node->parent)
                {
                  new_parent->note_children_increased(outline_view);
                  old_parent->note_children_decreased(outline_view);
                }
              else if ((old_prev != node->prev_sibling) ||
                       (old_next != node->next_sibling))
                old_parent->note_children_changed(outline_view);
              else if (label_changed)
                node->note_label_changed(outline_view);
            }
        }
    }
  
  for (int n=0; n < KDMS_CATALOG_NUM_TYPES; n++)
    if (root_nodes[n] != NULL)
      {
        root_nodes[n]->reflect_changes_part1(outline_view);
        root_nodes[n]->reflect_changes_part2(outline_view);
      }
  if (selected_metanode.exists() && !selected_metanode.is_deleted())
    [self select_matching_metanode:selected_metanode];
}

/*****************************************************************************/
/*                          kdms_catalog:dealloc                             */
/*****************************************************************************/

-(void)dealloc
{
  for (int n=0; n < KDMS_CATALOG_NUM_TYPES; n++)
    if (root_nodes[n] != NULL)
      delete root_nodes[n];
  [super dealloc];
}

/*****************************************************************************/
/*                   kdms_catalog:get_selected_metanode                      */
/*****************************************************************************/

-(jpx_metanode)get_selected_metanode
{
  jpx_metanode result;
  NSInteger idx = [outline_view selectedRow];
  if (idx >= 0)
    {
      kdms_catalog_item *item = (kdms_catalog_item *) [outline_view itemAtRow:idx];
      if (item != nil)
        {
          kdms_catalog_node *node = [item get_node];
          if (node != NULL)
            result = node->metanode;
        }
    }
  return result;
}

/*****************************************************************************/
/*                  kdms_catalog:select_matching_metanode                    */
/*****************************************************************************/

-(void)select_matching_metanode:(jpx_metanode)metanode
{
  if (!metanode)
    return;
  kdms_catalog_node *node = (kdms_catalog_node *)metanode.get_state_ref();
  if ((node == NULL) || (node->metanode != metanode))
    return;
  
  // Find which nodes need to be expanded
  kdms_catalog_node *scan;
  bool needs_attention=false, already_expanded=false;
  for (scan=node->parent; scan != NULL; scan=scan->parent)
    {
      if (needs_attention)
        scan->descendant_needs_attention = true;
      if (already_expanded || [outline_view isItemExpanded:scan->item])
        already_expanded = true;
      else
        scan->needs_expand = needs_attention = true;
      if ((scan->parent == NULL) && needs_attention)
        scan->reflect_changes_part2(outline_view);
    }

  // Now select the node
  NSInteger idx = [outline_view rowForItem:node->item];
  if (idx >= 0)
    {
      NSIndexSet *rows = [NSIndexSet indexSetWithIndex:idx];
      [outline_view selectRowIndexes:rows byExtendingSelection:NO];
    }
}

/*****************************************************************************/
/*                     kdms_catalog:user_double_click                        */
/*****************************************************************************/

-(IBAction)user_double_click:(NSBrowser *)sender
{
  jpx_metanode metanode = [self get_selected_metanode];
  if (metanode.exists() && (renderer != NULL))
    renderer->show_imagery_for_metanode(metanode);
}

/*****************************************************************************/
/*                 kdms_catalog:outlineView:child:ofItem                     */
/*****************************************************************************/

-(id)outlineView:(NSOutlineView *)view child:(NSInteger)idx ofItem:(id)item
{
  if (item == nil)
    {
      if ((idx < 0) || (idx >= KDMS_CATALOG_NUM_TYPES) ||
          (root_nodes[idx]==NULL))
        return nil;
      return root_nodes[idx]->item;
    }
  kdms_catalog_node *node = [item get_node];
  if ((idx < 0) || (idx >= node->num_children))
    return nil;
  kdms_catalog_node *child;
  for (child=node->children; idx > 0; idx--)
    child=child->next_sibling;
  return child->item;
}

/*****************************************************************************/
/*               kdms_catalog:outlineView:isItemExpandable                   */
/*****************************************************************************/

-(BOOL)outlineView:(NSOutlineView *)view isItemExpandable:(id)item
{
  if (item == nil)
    return TRUE;
  kdms_catalog_node *node = [item get_node];
  return (node->num_children > 0)?YES:NO;  
}

/*****************************************************************************/
/*            kdms_catalog:outlineView:numberOfChildrenOfItem                */
/*****************************************************************************/

-(NSInteger) outlineView:(NSOutlineView *)view numberOfChildrenOfItem:(id)item
{
  if (item == nil)
    return KDMS_CATALOG_NUM_TYPES;
  kdms_catalog_node *node = [item get_node];
  return (NSInteger)(node->num_children);
}

/*****************************************************************************/
/*        kdms_catalog:outlineView:objectValueForTableColumn:byItem          */
/*****************************************************************************/

-(id)outlineView:(NSOutlineView *)view
     objectValueForTableColumn:(NSTableColumn *)table_column byItem:(id)item
{
  kdms_catalog_node *node = [item get_node];
  return node->label;
}

@end // kdms_catalog
