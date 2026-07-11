import React from 'react';
import {
  View,
  StyleSheet,
  requireNativeComponent,
  Platform,
  type ViewProps,
  type ViewStyle,
} from 'react-native';

type NativeBackdropBlurViewProps = ViewProps & {
  blurRadius?: number;
  overlayColor?: string;
  downsample?: number;
  blurEnabled?: boolean;
};

const NativeBackdropBlurView =
  Platform.OS === 'android'
    ? requireNativeComponent<NativeBackdropBlurViewProps>('BackdropBlurView')
    : View;

export interface BackdropBlurViewProps extends ViewProps {
  /** Blur strength. On API 31+ this is the GPU RenderEffect radius; below, it's scaled to the downsampled capture. */
  blurRadius?: number;
  /** Tint drawn over the blurred backdrop (e.g. 'rgba(255,255,255,0.15)'). On API 21–23 this is the only effect. */
  overlayColor?: string;
  /** Capture at 1/downsample resolution. Omit to let the native side pick per API level (4 on 31+, 8 below). */
  downsample?: number;
  /** Pause/resume the capture loop (e.g. to save power while the panel is hidden). */
  blurEnabled?: boolean;
  style?: ViewStyle;
  children?: React.ReactNode;
}

/**
 * A panel that blurs whatever GPU SurfaceView sits behind it — a glassmorphic
 * panel over live GPU content (a map, a video, a camera preview). Unlike
 * {@link BlurView} (which blurs its own children), this does *backdrop* blur:
 * the SurfaceView behind the panel is blurred, the panel's children render
 * sharp on top.
 *
 * Android only. The blur backend is chosen per OS version (GPU RenderEffect on
 * 12+, C++/NEON on 7–11, a translucent scrim below that). On other platforms
 * it renders as a plain View. See the native `BackdropBlurView` for the
 * capture/latency tradeoffs of blurring moving content.
 */
const BackdropBlurView: React.FC<BackdropBlurViewProps> = ({
  blurRadius = 16,
  overlayColor = 'transparent',
  downsample,
  blurEnabled = true,
  style,
  children,
  ...rest
}) => {
  if (Platform.OS === 'android') {
    return (
      <NativeBackdropBlurView
        blurRadius={blurRadius}
        overlayColor={overlayColor}
        downsample={downsample}
        blurEnabled={blurEnabled}
        style={[styles.container, style]}
        {...rest}
      >
        {children}
      </NativeBackdropBlurView>
    );
  }

  return (
    <View style={[styles.container, style]} {...rest}>
      {children}
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    overflow: 'hidden',
  },
});

export default BackdropBlurView;
