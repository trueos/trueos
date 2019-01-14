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

    stage('Nullfs') {
      steps {
        sh 'mkdir -p ${SRCROOT} || true'
        sh 'mount_nullfs ${WORKSPACE} ${SRCROOT}'
      }
    }

    stage('Pre-Clean') {
      steps {
        sh 'rm -rf ${WORKSPACE}/artifacts'
        sh 'mkdir -p ${WORKSPACE}/artifacts/repo'
        sh 'rm -rf /usr/obj${SRCROOT} || true'
        sh 'chflags -R noschg /usr/obj${SRCROOT} || true'
        sh 'rm -rf /usr/obj${SRCROOT} || true'
      }
    }
	  
    stage('World') {
      post {
        always {
          archiveArtifacts artifacts: 'artifacts/**', fingerprint: true
        }
        failure {
	  sh 'tail -n 200 ${WORKSPACE}/artifacts/world.log'
        }
      }
      steps {
        sh 'cd ${SRCROOT} && make -j $(sysctl -n hw.ncpu) buildworld >${WORKSPACE}/artifacts/world.log 2>&1'
      }
    }
	  
    stage('Kernel') {
      post {
        always {
          archiveArtifacts artifacts: 'artifacts/**', fingerprint: true
        }
        failure {
	  sh 'tail -n 200 ${WORKSPACE}/artifacts/kernel.log'
        }
      }
      steps {
        sh 'cd ${SRCROOT} && make -j $(sysctl -n hw.ncpu) buildkernel >${WORKSPACE}/artifacts/kernel.log 2>&1'
      }
    }
	  
    stage('Base Packages') {
      post {
        always {
          archiveArtifacts artifacts: 'artifacts/**', fingerprint: true
        }
        failure {
	  sh 'tail -n 200 ${WORKSPACE}/artifacts/packages.log'
        }
      }
      environment {
           PKGSIGNKEY = credentials('a50f9ddd-1460-4951-a304-ddbf6f2f7990')
      }
      steps {
        sh 'cd ${SRCROOT} && make packages -j 16 -DDB_FROM_SRC >${WORKSPACE}/artifacts/packages.log 2>&1'
      }
    }
	  
    stage('Release') {
      post {
        always {
          archiveArtifacts artifacts: 'artifacts/**', fingerprint: true
        }
        failure {
	  sh 'tail -n 200 ${WORKSPACE}/artifacts/release.log'
        }
      }
	    
      steps {
        sh 'cd ${SRCROOT}/release && make clean || true'
        sh 'cd ${SRCROOT}/release && make iso >${WORKSPACE}/artifacts/release.log 2>&1'
        sh 'cp /usr/obj${SRCROOT}/amd64.amd64/release/*.iso ${WORKSPACE}/artifacts'
        sh 'cp /usr/obj${SRCROOT}/amd64.amd64/release/*.img ${WORKSPACE}/artifacts'
        sh 'cp -r /usr/obj${SRCROOT}/repo/* ${WORKSPACE}/artifacts/repo/'
      }
    }
  }
	
  post {
    always {
      archiveArtifacts artifacts: 'artifacts/**', fingerprint: true
      echo "*** Cleaning up ***"
      sh 'rm -rf /usr/obj${SRCROOT} || true'
      sh 'chflags -R noschg /usr/obj${SRCROOT} || true'
      sh 'rm -rf /usr/obj${SRCROOT} || true'
      sh 'umount -f ${SRCROOT} || true'
      script {
        cleanWs notFailBuild: true
      }
    }
  }
}
