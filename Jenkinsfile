/*

This is the default Jenkins Pipeline config used to test pull-requests
on GitHub trueos/trueos repo.

*/

pipeline {
  agent { label 'TrueOS-PR' }

  environment {
    GH_ORG = 'trueos'
    GH_REPO = 'trueos'
    SRCROOT = '/usr/trueos-ci'
    TRUEOS_VERSION = 'JenkinsCI'
  }

  stages {
    stage('Queued') {
        agent {
        label 'JenkinsMaster'
      }
      steps {
        echo "Build queued"
      }
    }

    stage('Checkout') {
      steps {
        checkout scm
      }
    }

    stage('Pre-Clean') {
      steps {
        sh 'rm -rf ${WORKSPACE}/artifacts'
        sh 'mkdir ${WORKSPACE}/artifacts'
        sh 'rm -rf ${WORKSPACE}/destdir'
        sh 'mkdir ${WORKSPACE}/destdir'
      }
    }
	  
    stage('World') {
      post {
        always {
          archiveArtifacts artifacts: 'artifacts/**', fingerprint: true
        }
        failure {
	  sh 'tail -n 500 ${WORKSPACE}/artifacts/world.log'
        }
      }
      steps {
        sh 'make -j $(sysctl -n hw.ncpu) buildworld DESTDIR=${WORKSPACE}/destdir >${WORKSPACE}/artifacts/world.log 2>&1'
        sh 'make -j $(sysctl -n hw.ncpu) installworld DESTDIR=${WORKSPACE}/destdir >>${WORKSPACE}/artifacts/world.log 2>&1'
      }
    }
	  
    stage('Kernel') {
      post {
        always {
          archiveArtifacts artifacts: 'artifacts/**', fingerprint: true
        }
        failure {
	  sh 'tail -n 500 ${WORKSPACE}/artifacts/kernel.log'
        }
      }
      steps {
        sh 'make -j $(sysctl -n hw.ncpu) buildkernel DESTDIR=${WORKSPACE}/destdir >${WORKSPACE}/artifacts/kernel.log 2>&1'
        sh 'make -j $(sysctl -n hw.ncpu) installkernel DESTDIR=${WORKSPACE}/destdir >>${WORKSPACE}/artifacts/kernel.log 2>&1'
      }
    }
  }
  post {
    always {
      script {
        cleanWs notFailBuild: true
      }
      sh 'rm -rf DESTDIR=${WORKSPACE}/destdir || true'
      sh 'chflags -R noschg DESTDIR=${WORKSPACE}/destdir || true'
      sh 'rm -rf DESTDIR=${WORKSPACE}/destdir'
    }
  }
}
