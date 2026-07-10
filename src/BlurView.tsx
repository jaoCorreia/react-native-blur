import React from 'react';
import {
  View,
  StyleSheet,
  requireNativeComponent,
  Platform,
  type ViewProps,
  type ViewStyle,
} from 'react-native';

type NativeBlurViewProps = ViewProps & {
  blurRadius: number;
  overlayColor?: string;
};

const NativeBlurView =
  Platform.OS === 'android'
    ? requireNativeComponent<NativeBlurViewProps>('BlurView')
    : View;

export interface BlurViewProps extends ViewProps {
  blurRadius?: number;
  overlayColor?: string;
  style?: ViewStyle;
  children?: React.ReactNode;
}

const BlurView: React.FC<BlurViewProps> = ({
  blurRadius = 10,
  overlayColor = 'transparent',
  style,
  children,
  ...rest
}) => {
  if (Platform.OS === 'android') {
    return (
      <NativeBlurView
        blurRadius={blurRadius}
        overlayColor={overlayColor}
        style={[styles.container, style]}
        {...rest}
      >
        {children}
      </NativeBlurView>
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

export default BlurView;
