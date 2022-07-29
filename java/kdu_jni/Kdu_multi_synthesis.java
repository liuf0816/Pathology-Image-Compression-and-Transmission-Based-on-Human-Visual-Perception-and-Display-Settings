package kdu_jni;

public class Kdu_multi_synthesis {
  static {
    System.loadLibrary("kdu_jni");
    Native_init_class();
  }
  private static native void Native_init_class();
  public long _native_ptr = 0;
  public native boolean Exists() throws KduException;
  public native long Create(Kdu_codestream _codestream, Kdu_tile _tile, boolean _force_precise, boolean _skip_ycc, boolean _want_fastest, int _processing_stripe_height, Kdu_thread_env _env, long _env_queue, boolean _double_buffering) throws KduException;
  public long Create(Kdu_codestream _codestream, Kdu_tile _tile) throws KduException
  {
    Kdu_thread_env env = null;
    return Create(_codestream,_tile,(boolean) false,(boolean) false,(boolean) false,(int) 1,env,0,(boolean) false);
  }
  public long Create(Kdu_codestream _codestream, Kdu_tile _tile, boolean _force_precise) throws KduException
  {
    Kdu_thread_env env = null;
    return Create(_codestream,_tile,_force_precise,(boolean) false,(boolean) false,(int) 1,env,0,(boolean) false);
  }
  public long Create(Kdu_codestream _codestream, Kdu_tile _tile, boolean _force_precise, boolean _skip_ycc) throws KduException
  {
    Kdu_thread_env env = null;
    return Create(_codestream,_tile,_force_precise,_skip_ycc,(boolean) false,(int) 1,env,0,(boolean) false);
  }
  public long Create(Kdu_codestream _codestream, Kdu_tile _tile, boolean _force_precise, boolean _skip_ycc, boolean _want_fastest) throws KduException
  {
    Kdu_thread_env env = null;
    return Create(_codestream,_tile,_force_precise,_skip_ycc,_want_fastest,(int) 1,env,0,(boolean) false);
  }
  public long Create(Kdu_codestream _codestream, Kdu_tile _tile, boolean _force_precise, boolean _skip_ycc, boolean _want_fastest, int _processing_stripe_height) throws KduException
  {
    Kdu_thread_env env = null;
    return Create(_codestream,_tile,_force_precise,_skip_ycc,_want_fastest,_processing_stripe_height,env,0,(boolean) false);
  }
  public long Create(Kdu_codestream _codestream, Kdu_tile _tile, boolean _force_precise, boolean _skip_ycc, boolean _want_fastest, int _processing_stripe_height, Kdu_thread_env _env) throws KduException
  {
    return Create(_codestream,_tile,_force_precise,_skip_ycc,_want_fastest,_processing_stripe_height,_env,0,(boolean) false);
  }
  public long Create(Kdu_codestream _codestream, Kdu_tile _tile, boolean _force_precise, boolean _skip_ycc, boolean _want_fastest, int _processing_stripe_height, Kdu_thread_env _env, long _env_queue) throws KduException
  {
    return Create(_codestream,_tile,_force_precise,_skip_ycc,_want_fastest,_processing_stripe_height,_env,_env_queue,(boolean) false);
  }
  public native void Destroy(Kdu_thread_env _env) throws KduException;
  public void Destroy() throws KduException
  {
    Kdu_thread_env env = null;
    Destroy(env);
  }
  public native Kdu_coords Get_size(int _comp_idx) throws KduException;
  public native Kdu_line_buf Get_line(int _comp_idx, Kdu_thread_env _env) throws KduException;
  public Kdu_line_buf Get_line(int _comp_idx) throws KduException
  {
    Kdu_thread_env env = null;
    return Get_line(_comp_idx,env);
  }
  public native boolean Is_line_precise(int _comp_idx) throws KduException;
  public native boolean Is_line_absolute(int _comp_idx) throws KduException;
}
