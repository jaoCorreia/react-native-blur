import BlurView from './BlurView';
import BackdropBlurView from './BackdropBlurView';
import NativeBlur, { isTurboModuleAvailable } from './NativeBlur';

export { BlurView, BackdropBlurView, NativeBlur, isTurboModuleAvailable };

export type { BlurViewProps } from './BlurView';
export type { BackdropBlurViewProps } from './BackdropBlurView';

export default BlurView;
