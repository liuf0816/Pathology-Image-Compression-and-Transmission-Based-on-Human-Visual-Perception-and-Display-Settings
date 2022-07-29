package kdu_jni;

public class Jp2_dimensions {
  static {
    System.loadLibrary("kdu_jni");
    Native_init_class();
  }
  private static native void Native_init_class();
  public long _native_ptr = 0;
  public native boolean Exists() throws KduException;
  public native void Copy(Jp2_dimensions _src) throws KduException;
  public native void Init(Kdu_coords _size, int _num_components, boolean _unknown_space, int _compression_type) throws KduException;
  public native void Init(Siz_params _siz, boolean _unknown_space) throws KduException;
  public void Init(Siz_params _siz) throws KduException
  {
    Init(_siz,(boolean) true);
  }
  public native void Finalize_compatibility(Kdu_params _root) throws KduException;
  public native void Finalize_compatibility(Jp2_dimensions _src) throws KduException;
  public native void Set_precision(int _component_idx, int _bit_depth, boolean _is_signed) throws KduException;
  public void Set_precision(int _component_idx, int _bit_depth) throws KduException
  {
    Set_precision(_component_idx,_bit_depth,(boolean) false);
  }
  public native void Set_ipr_box_available() throws KduException;
  public native Kdu_coords Get_size() throws KduException;
  public native int Get_num_components() throws KduException;
  public native boolean Colour_space_known() throws KduException;
  public native int Get_bit_depth(int _component_idx) throws KduException;
  public native boolean Get_signed(int _component_idx) throws KduException;
  public native int Get_compression_type() throws KduException;
  public native boolean Is_ipr_box_available() throws KduException;
}
