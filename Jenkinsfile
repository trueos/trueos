pipeline {
  agent { label 'TrueOS-World' }

  environment {
    GH_ORG = 'trueos'
    GH_REPO = 'freebsd'
  }
  stages {
    stage('Checkout') {
      steps {
        checkout scm
      }
    }

    stage('Pre-Clean') {
      steps {
        sh 'make clean'
      }
    }
    stage('World') {
      steps {
        sh 'make -j32 buildworld'
      }
    }
    stage('Kernel') {
      steps {
        sh 'make -j32 buildkernel'
      }
    }
    stage('Packages') {
      steps {
        sh 'make -j32 packages'
      }
    }
    stage('Post-Clean') {
      steps {
        sh 'make clean'
      }
    }
  }
}
