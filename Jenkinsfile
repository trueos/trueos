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
    stage('DVD Release') {
      post {
        success {
          archiveArtifacts artifacts: 'artifacts/*', fingerprint: true
        }
      }
      steps {
        sh 'rm -rf ${WORKSPACE}/artifacts'
        sh 'cd release && make release'
        sh 'mkdir -p ${WORKSPACE}/artifacts'
        sh 'cp /usr/obj${WORKSPACE}/amd64.amd64/release/*.iso ${WORKSPACE}/artifacts'
        sh 'cp /usr/obj${WORKSPACE}/amd64.amd64/release/*.img ${WORKSPACE}/artifacts'
        sh 'cp /usr/obj${WORKSPACE}/amd64.amd64/release/*.txz ${WORKSPACE}/artifacts'
      }
    }
    stage('Post-Clean') {
      steps {
        sh 'make clean'
        sh 'cd release && make clean'
        sh 'rm -rf /usr/obj${WORKSPACE}/repo'
      }
    }
  }
}
