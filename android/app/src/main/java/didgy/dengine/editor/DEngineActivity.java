// TODO: Implement filtering, like only enable . - 1-9 for example. DONE.
// TODO: Implement filtering for -, it should only be possible to be entered if it's the first character in the string. DONE.
// TODO: Implement the filtering mentioned above for desktop.
// TODO: Detect when keyboard appears/disappears, possibly through onGlobalLayoutChange. CANCELLED.
// TODO: Implement the "Submit" button functionality stuff. DONE.
// TODO: Fix being only able to open keyboard once then never again. DONE.
// TODO: Add support for integer type input, needs differing text filters. DONE.
// TODO: Fix issue where switching between input fields can cause keyboard to close depending on execution order in code.

package didgy.dengine.editor;

import android.app.NativeActivity;
import android.content.Context;
import android.content.res.Configuration;
import android.os.Bundle;
import android.text.Editable;
import android.text.InputFilter;
import android.text.InputType;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.util.Log;
import android.view.KeyEvent;
import android.view.SurfaceHolder;
import android.view.View;
import android.view.WindowInsets;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;

public class DEngineActivity extends NativeActivity  {

    static public final int SOFT_INPUT_FILTER_INTEGER = 0;
    static public final int SOFT_INPUT_FILTER_UNSIGNED_INTEGER = 1;
    static public final int SOFT_INPUT_FILTER_FLOAT = 2;

    static {
        System.loadLibrary("dengine");
    }

    public native void nativeInit();

    public native void nativeOnCharInput(int utfValue);
    public native void nativeOnCharEnter();
    public native void nativeOnCharRemove();

    public native void nativeUpdateWindowContentRect(int top, int bottom, int left, int right);

    public int mInputConnectionCounter = 0;
    public int softInputFilter = 0;
    public NativeContentView mNativeContentView = null;
    public InputEditable mEditable = null;
    public InputMethodManager mIMM = null;

    @Override
    protected void onCreate(Bundle savedState) {
        super.onCreate(savedState);

        mIMM = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
        assert mIMM != null;

        mEditable = new InputEditable(this, "");
        mNativeContentView = new NativeContentView(this);

        setContentView(mNativeContentView);
        //mNativeContentView.setFocusableInTouchMode(true);
        //mNativeContentView.requestFocus();
        mNativeContentView.getViewTreeObserver().addOnGlobalLayoutListener(this);

        nativeInit();
    }

    @Override
    protected void onStart() {
        super.onStart();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
    }

    @Override
    public void onGlobalLayout() {
        super.onGlobalLayout();

        // TODO: The on-screen display is changed in this event.
    }

    @Override
    public void onContentChanged() {
        super.onContentChanged();
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        super.surfaceCreated(holder);

        android.graphics.Rect test = new android.graphics.Rect();
        mNativeContentView.getWindowVisibleDisplayFrame(test);

        nativeUpdateWindowContentRect(test.top, test.bottom, test.left, test.right);

        android.graphics.Rect x = new android.graphics.Rect();
        mNativeContentView.getDrawingRect(x);

        android.graphics.Point pointTest = new android.graphics.Point();
        mNativeContentView.getDisplay().getSize(pointTest);

        WindowInsets yo = mNativeContentView.getRootWindowInsets();
    }

    public void openSoftInput(String text, final int softInputFilter) {
        class OpenSoftInputRunnable implements Runnable {
            DEngineActivity activity;
            int softInputFilter;
            String innerText;
            OpenSoftInputRunnable(DEngineActivity inActivity, String text, int filter) {
                activity = inActivity;
                this.softInputFilter = filter;
                innerText = text;
            }
            public void run() {
                activity.softInputFilter = softInputFilter;
                mEditable = new InputEditable(activity, innerText);

                mNativeContentView.setFocusableInTouchMode(true);
                mNativeContentView.requestFocus();
                mIMM.showSoftInput(mNativeContentView, 0);
            }
        }
        OpenSoftInputRunnable test = new OpenSoftInputRunnable(this, text, softInputFilter);
        runOnUiThread(test);
    }

    public void hideSoftInput()
    {
        class HideSoftInputRunnable implements Runnable {
            DEngineActivity activity;
            HideSoftInputRunnable() {
            }
            public void run() {
                mEditable = null;
                mIMM.hideSoftInputFromWindow(mNativeContentView.getWindowToken(), 0);
            }
        }
        HideSoftInputRunnable test = new HideSoftInputRunnable();
        runOnUiThread(test);
    }

    public int getCurrentOrientation()
    {
        return getResources().getConfiguration().orientation;
    }

    static boolean contains(CharSequence seq, char character) {
        int length = seq.length();
        for (int i = 0; i < length; i++) {
            if (seq.charAt(i) == character) {
                return true;
            }
        }
        return false;
    }

    static class InputEditable extends SpannableStringBuilder  {

        public DEngineActivity mActivity;

        public InputEditable(DEngineActivity activity, String string) {
            super(string);
            this.mActivity = activity;

            InputFilter[] inputFilters = null;
            switch (mActivity.softInputFilter)
            {
                case SOFT_INPUT_FILTER_INTEGER:
                    inputFilters = new InputFilter[]{ new SignedIntegerTextFilter() };
                    break;
                case SOFT_INPUT_FILTER_UNSIGNED_INTEGER:
                    inputFilters = new InputFilter[]{ new UnsignedIntegerTextFilter(), new MinusFilter() };
                    break;
                case SOFT_INPUT_FILTER_FLOAT:
                    inputFilters = new InputFilter[]{ new FloatTextFilter(), new DotFilter(), new MinusFilter() };
                    break;
            }

            this.setFilters(inputFilters);
        }

        @Override
        public SpannableStringBuilder replace(int st, int en, CharSequence source, int start, int end) {

            int prevLength = length();

            SpannableStringBuilder returnVal = super.replace(st, en, source, start, end);

            int newLength = length();

            if (prevLength != newLength) { // Length is same. We assume nothing changed (for now).
                String logString = "Current input field value: ";
                String currentValueString = this.toString();
                if (currentValueString.equals(""))
                    logString += "Empty";
                else
                    logString += currentValueString;
                Log.e("DEngineActivity", logString);

                if (st < prevLength) {
                    if (source == null || source == "") {
                        for (int i = st; i < prevLength; i++) {
                            mActivity.nativeOnCharRemove();
                        }
                    }
                }
                else {
                    if (source != null && source != "") {
                        int j = start;
                        while (j < end) {
                            mActivity.nativeOnCharInput(source.charAt(j));
                            j++;
                        }
                    }
                }
            }

            return returnVal;
        }

        static class SignedIntegerTextFilter implements InputFilter {
            @Override
            public CharSequence filter(CharSequence source, int start, int end, Spanned dest, int dstart, int dend) {
                boolean sourceHasInvalidChar = false;
                int sourceLength = end - start;
                for (int i = start; i < end; i++) {
                    char c = source.charAt(i);
                    if (!(48 <= c && c <= 57) &&
                        c != '-') {
                        sourceHasInvalidChar = true;
                        break;
                    }
                }
                if (sourceHasInvalidChar) {
                    StringBuilder returnVal = new StringBuilder(sourceLength);
                    for (int i = start; i < end; i++) {
                        char c = source.charAt(i);
                        if ((48 <= c && c <= 57) ||
                            c == '-') {
                            returnVal.append(c);
                        }
                    }
                    return returnVal;
                }
                else
                    return null;
            }
        }

        static class UnsignedIntegerTextFilter implements InputFilter {
            @Override
            public CharSequence filter(CharSequence source, int start, int end, Spanned dest, int dstart, int dend) {
                boolean sourceHasInvalidChar = false;
                int sourceLength = end - start;
                for (int i = start; i < end; i++) {
                    char c = source.charAt(i);
                    if (!(48 <= c && c <= 57)) {
                        sourceHasInvalidChar = true;
                        break;
                    }
                }
                if (sourceHasInvalidChar) {
                    StringBuilder returnVal = new StringBuilder(sourceLength);
                    for (int i = start; i < end; i++) {
                        char c = source.charAt(i);
                        if (48 <= c && c <= 57) {
                            returnVal.append(c);
                        }
                    }
                    return returnVal;
                }
                else
                    return null;
            }
        }

        static class FloatTextFilter implements InputFilter {
            @Override
            public CharSequence filter(CharSequence source, int start, int end, Spanned dest, int dstart, int dend) {
                boolean sourceHasInvalidChar = false;
                int sourceLength = end - start;
                for (int i = start; i < end; i++) {
                    char c = source.charAt(i);
                    if (!(48 <= c && c <= 57) &&
                        c != '.' &&
                        c != '-') {
                        sourceHasInvalidChar = true;
                        break;
                    }
                }
                if (sourceHasInvalidChar) {
                    StringBuilder returnVal = new StringBuilder(sourceLength);
                    for (int i = start; i < end; i++) {
                        char c = source.charAt(i);
                        if ((48 <= c && c <= 57) ||
                            c == '.' ||
                            c == '-')
                            returnVal.append(c);
                    }
                    return returnVal;
                }
                else
                    return null;
            }
        }

        static class DotFilter implements InputFilter {
            @Override
            public CharSequence filter(CharSequence source, int start, int end, Spanned dest, int dstart, int dend) {
                if (contains(source.subSequence(start, end), '.')) {
                    if (contains(dest, '.')) {
                        // We need to build a sequence with no dot.
                        StringBuilder returnVal = new StringBuilder(end - start);
                        for (int i = start; i < end; i++) {
                            char c = source.charAt(i);
                            if (c != '.')
                                returnVal.append(c);
                        }
                        return returnVal;
                    }
                }
                return null;
            }
        }

        static class MinusFilter implements InputFilter {
            @Override
            public CharSequence filter(CharSequence source, int start, int end, Spanned dest, int dstart, int dend) {
                if (contains(source.subSequence(start, end), '-')) {
                    if (dest.length() != 0) {
                        return "";
                    }
                }
                return null;
            }
        }
    }

    static class NativeContentView extends View {
        static class NativeInputConnection extends BaseInputConnection {

            public DEngineActivity mActivity;
            public int mInputConnectionID;

            public NativeInputConnection(NativeContentView targetView, DEngineActivity activity) {
                super(targetView, true);

                this.mActivity = activity;

                mInputConnectionID = mActivity.mInputConnectionCounter;
                mActivity.mInputConnectionCounter++;
            }

            @Override
            public ExtractedText getExtractedText(ExtractedTextRequest request, int flags) {
                ExtractedText returnVal = new ExtractedText();
                returnVal.text = mActivity.mEditable;
                returnVal.selectionStart = returnVal.text.length();
                returnVal.selectionEnd = returnVal.text.length();
                return returnVal;
            }

            @Override
            public Editable getEditable() {
                return mActivity.mEditable;
            }

            @Override
            public boolean sendKeyEvent(KeyEvent event) {
                int keyAction = event.getAction();
                if (keyAction == KeyEvent.ACTION_UP)
                {
                    // The dot and minus sign is handled by the commit-text event
                    // So we use a filter for that.

                    int test = event.getUnicodeChar();
                    if (48 <= test && test <= 57) {
                        mActivity.mEditable.append((char)test);
                        this.setSelection(mActivity.mEditable.length(), mActivity.mEditable.length());
                    }

                    int keyCode = event.getKeyCode();
                    if (keyCode == KeyEvent.KEYCODE_DEL) {
                        if (mActivity.mEditable.length() > 0) {
                            mActivity.mEditable.delete(mActivity.mEditable.length() - 1, mActivity.mEditable.length());
                            this.setSelection(mActivity.mEditable.length(), mActivity.mEditable.length());
                        }
                    }
                    else if (keyCode == KeyEvent.KEYCODE_ENTER) {
                        mActivity.nativeOnCharEnter();
                    }
                }

                return super.sendKeyEvent(event);
            }
        }

        public DEngineActivity mActivity;

        public NativeContentView(DEngineActivity activity) {
            super(activity);

            this.mActivity = activity;
        }

        public NativeContentView(Context activity) {
            super(activity);

            this.mActivity = null;
        }

        @Override
        public WindowInsets onApplyWindowInsets(WindowInsets insets) {
            return super.onApplyWindowInsets(insets);
        }

        @Override
        public InputConnection onCreateInputConnection(EditorInfo outAttrs) {

            outAttrs.initialSelStart = mActivity.mEditable.length();
            outAttrs.initialSelEnd = mActivity.mEditable.length();

            switch (mActivity.softInputFilter)
            {
                case SOFT_INPUT_FILTER_INTEGER:
                    outAttrs.inputType = InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_SIGNED;
                    break;
                case SOFT_INPUT_FILTER_UNSIGNED_INTEGER:
                    outAttrs.inputType = InputType.TYPE_CLASS_NUMBER;
                    break;
                case SOFT_INPUT_FILTER_FLOAT:
                    outAttrs.inputType = InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL | InputType.TYPE_NUMBER_FLAG_SIGNED;
                    break;
            }

            //outAttrs.inputType = InputType.TYPE_CLASS_TEXT;
            return new NativeInputConnection(this, mActivity);
        }

        @Override
        public boolean onCheckIsTextEditor() {
            // This means we don't want the IME to automatically pop up
            // when opening this View.
            return false;
        }

        @Override
        protected void finalize() throws Throwable {

            super.finalize();
        }
    }
}