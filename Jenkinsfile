pipeline {
  agent { label 'TrueOS-World' }
  checkout scm

  environment {
    GH_ORG = 'trueos'
    GH_REPO = 'freebsd'
  }
  stages {
    stage('Clean') {
      steps {
        sh 'make clean'
      }
    }
    stage('World') {
      steps {
        sh 'make -j24 buildworld'
      }
    }
    stage('Kernel') {
      steps {
        sh 'make -j24 buildkernel'
      }
    }
    stage('Packages') {
      steps {
        sh 'make -j24 packages'
      }
    }
    stage('Clean') {
      steps {
        sh 'make clean'
      }
    }
  }
}
