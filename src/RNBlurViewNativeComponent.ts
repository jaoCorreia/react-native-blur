import type { HostComponent, ViewProps } from 'react-native';
import type { BubblingEventHandler } from 'react-native/Libraries/Types/CodegenTypes';
import codegenNativeComponent from 'react-native/Libraries/Utilities/codegenNativeComponent';

export interface NativeProps extends ViewProps {
  blurRadius?: number;
  overlayColor?: string;
  onBlurApplied?: BubblingEventHandler<Readonly<{ radius: number }>>;
}

export default codegenNativeComponent<NativeProps>('BlurView', {
  interfaceOnly: true,
}) as HostComponent<NativeProps>;
