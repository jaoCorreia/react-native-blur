const path = require('path');

module.exports = {
  dependency: {
    platforms: {
      android: {
        sourceDir: path.join(__dirname, 'android'),
        packageImportPath: 'import com.blur.BlurPackage;',
        packageInstance: 'new BlurPackage()',
      },
      ios: null,
    },
  },
};
