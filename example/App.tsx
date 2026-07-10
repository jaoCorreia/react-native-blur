import React from 'react';
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  Image,
  SafeAreaView,
} from 'react-native';
import { BlurView } from 'react-native-blur';

const App: React.FC = () => {
  return (
    <SafeAreaView style={styles.safe}>
      <ScrollView contentContainerStyle={styles.scrollContent}>
        <Text style={styles.heading}>react-native-blur</Text>
        <Text style={styles.subtitle}>C++ Gaussian Blur with NEON SIMD</Text>

        <View style={styles.demoSection}>
          <Text style={styles.demoTitle}>Radius: 5</Text>
          <View style={styles.demoContainer}>
            <Image
              source={{ uri: 'https://picsum.photos/300/200' }}
              style={styles.image}
              resizeMode="cover"
            />
            <BlurView blurRadius={5} style={StyleSheet.absoluteFill}>
              <View style={styles.overlayContent}>
                <Text style={styles.overlayText}>Glassmorphism</Text>
              </View>
            </BlurView>
          </View>
        </View>

        <View style={styles.demoSection}>
          <Text style={styles.demoTitle}>Radius: 15</Text>
          <View style={styles.demoContainer}>
            <Image
              source={{ uri: 'https://picsum.photos/300/200' }}
              style={styles.image}
              resizeMode="cover"
            />
            <BlurView blurRadius={15} style={StyleSheet.absoluteFill}>
              <View style={styles.overlayContent}>
                <Text style={styles.overlayText}>Stronger Blur</Text>
              </View>
            </BlurView>
          </View>
        </View>

        <View style={styles.demoSection}>
          <Text style={styles.demoTitle}>Radius: 30 + Color Overlay</Text>
          <View style={styles.demoContainer}>
            <Image
              source={{ uri: 'https://picsum.photos/300/200' }}
              style={styles.image}
              resizeMode="cover"
            />
            <BlurView
              blurRadius={30}
              overlayColor="rgba(255,255,255,0.3)"
              style={StyleSheet.absoluteFill}
            >
              <View style={styles.overlayContent}>
                <Text style={styles.overlayText}>Heavy Blur</Text>
              </View>
            </BlurView>
          </View>
        </View>
      </ScrollView>
    </SafeAreaView>
  );
};

const styles = StyleSheet.create({
  safe: {
    flex: 1,
    backgroundColor: '#0d1117',
  },
  scrollContent: {
    padding: 20,
    paddingBottom: 60,
  },
  heading: {
    fontSize: 28,
    fontWeight: '700',
    color: '#f0f6fc',
    textAlign: 'center',
    marginTop: 10,
  },
  subtitle: {
    fontSize: 14,
    color: '#8b949e',
    textAlign: 'center',
    marginBottom: 30,
  },
  demoSection: {
    marginBottom: 30,
  },
  demoTitle: {
    fontSize: 16,
    color: '#c9d1d9',
    marginBottom: 8,
    fontWeight: '600',
  },
  demoContainer: {
    height: 200,
    borderRadius: 16,
    overflow: 'hidden',
    borderWidth: 1,
    borderColor: '#30363d',
  },
  image: {
    width: '100%',
    height: '100%',
  },
  overlayContent: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
  },
  overlayText: {
    fontSize: 24,
    fontWeight: '700',
    color: '#ffffff',
    textShadowColor: 'rgba(0,0,0,0.5)',
    textShadowOffset: { width: 1, height: 1 },
    textShadowRadius: 4,
  },
});

export default App;
