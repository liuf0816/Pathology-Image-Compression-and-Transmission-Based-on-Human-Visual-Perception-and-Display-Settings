package kdu_jni;

public class Jpx_metanode {
  static {
    System.loadLibrary("kdu_jni");
    Native_init_class();
  }
  private static native void Native_init_class();
  public long _native_ptr = 0;
  public native boolean Exists() throws KduException;
  public native boolean Get_numlist_info(int[] _num_codestreams, int[] _num_layers, boolean[] _applies_to_rendered_result) throws KduException;
  public native long Get_numlist_codestreams() throws KduException;
  public native long Get_numlist_layers() throws KduException;
  public native int Get_numlist_codestream(int _which) throws KduException;
  public native int Get_numlist_layer(int _which) throws KduException;
  public native boolean Test_numlist(int _codestream_idx, int _compositing_layer_idx, boolean _applies_to_rendered_result) throws KduException;
  public native int Get_num_regions() throws KduException;
  public native Jpx_roi Get_region(int _which) throws KduException;
  public native Kdu_dims Get_bounding_box() throws KduException;
  public native boolean Test_region(Kdu_dims _region) throws KduException;
  public native long Get_box_type() throws KduException;
  public native String Get_label() throws KduException;
  public native boolean Get_uuid(byte[] _uuid) throws KduException;
  public native boolean Open_existing(Jp2_input_box _box) throws KduException;
  public native boolean Count_descendants(int[] _count) throws KduException;
  public native Jpx_metanode Get_descendant(int _which) throws KduException;
  public native Jpx_metanode Get_parent() throws KduException;
  public native boolean Change_parent(Jpx_metanode _new_parent) throws KduException;
  public native Jpx_metanode Add_numlist(int _num_codestreams, int[] _codestream_indices, int _num_compositing_layers, int[] _layer_indices, boolean _applies_to_rendered_result) throws KduException;
  public native Jpx_metanode Add_regions(int _num_regions, Jpx_roi _regions) throws KduException;
  public native Jpx_metanode Add_label(String _text) throws KduException;
  public native void Change_to_label(String _text) throws KduException;
  public native Jpx_metanode Add_copy(Jpx_metanode _src, boolean _recursive) throws KduException;
  public native void Delete_node() throws KduException;
  public native boolean Is_changed() throws KduException;
  public native boolean Ancestor_changed() throws KduException;
  public native boolean Is_deleted() throws KduException;
  public native void Touch() throws KduException;
  public native long Get_state_ref() throws KduException;
}
