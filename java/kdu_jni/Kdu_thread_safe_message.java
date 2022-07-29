package kdu_jni;

public class Kdu_thread_safe_message extends Kdu_message {
  static {
    System.loadLibrary("kdu_jni");
    Native_init_class();
  }
  private static native void Native_init_class();
  protected Kdu_thread_safe_message(long ptr) {
    super(ptr);
  }
  public native void Native_destroy();
  public void finalize() {
    if ((_native_ptr & 1) != 0)
      { // Resource created and not donated
        Native_destroy();
      }
  }
  private static native long Native_create();
  public Kdu_thread_safe_message() {
    this(Native_create());
  }
  public native void Flush(boolean _end_of_message) throws KduException;
  public void Flush() throws KduException
  {
    Flush((boolean) false);
  }
  public native void Start_message() throws KduException;
}
