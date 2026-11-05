plugins { id("com.android.application") }
android {
    namespace = "com.arkaroy.occamfresh"
    compileSdk = 36
    buildFeatures { buildConfig = true }
    defaultConfig {
        applicationId = "com.arkaroy.occamfresh"
        minSdk = 28
        targetSdk = 36
        versionCode = 124
        versionName = "0.124-portrait-size-and-pool"
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
}
dependencies { compileOnly("de.robv.android.xposed:api:82") }
