/*

This is the default Jenkins Pipeline config used to test pull-requests
on GitHub trueos/trueos repo.

*/

pipeline {
  agent { label 'TrueOS-PR' }

  environment {
    GH_ORG = 'trueos'
    GH_REPO = 'trueos'
    SRCROOT = '/usr/trueos-src'
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
        sh 'cd ${SRCROOT} && make clean'
        sh 'cd ${SRCROOT}/release && make clean'
      }
    }
    stage('World') {
      steps {
        sh 'cd ${SRCROOT} && make -j32 buildworld'
      }
    }
    stage('Kernel') {
      steps {
        sh 'cd ${SRCROOT} && make -j32 buildkernel'
      }
    }
    stage('Base Packages') {
      environment {
           PKGSIGNKEY = credentials('a50f9ddd-1460-4951-a304-ddbf6f2f7990')
      }
      steps {
        sh 'cd ${SRCROOT} && make -j32 packages'
      }
    }
    stage('Ports') {
      environment {
           PKGSIGNKEY = credentials('a50f9ddd-1460-4951-a304-ddbf6f2f7990')
      }
      steps {
        sh 'cd ${SRCROOT}/release && make poudriere'
      }
    }
    stage('Release') {
      post {
        success {
          archiveArtifacts artifacts: 'artifacts/**', fingerprint: true
        }
      }
      steps {
        sh 'rm -rf ${WORKSPACE}/artifacts'
        sh 'cd ${SRCROOT}/release && make release'
        sh 'mkdir -p ${WORKSPACE}/artifacts/repo'
        sh 'cp /usr/obj${SRCROOT}/amd64.amd64/release/*.iso ${WORKSPACE}/artifacts'
        sh 'cp /usr/obj${SRCROOT}/amd64.amd64/release/*.txz ${WORKSPACE}/artifacts'
        sh 'cp /usr/obj${SRCROOT}/amd64.amd64/release/MANIFEST ${WORKSPACE}/artifacts'
        sh 'cp -r /usr/obj${SRCROOT}/repo/* ${WORKSPACE}/artifacts/repo/'
      }
    }
  }
  post {
    always {
      echo "*** Cleaning up ***"
      sh 'cd ${SRCROOT} && make clean >/dev/null 2>/dev/null'
      sh 'cd ${SRCROOT}/release && make clean >/dev/null 2>/dev/null '
      sh 'rm -rf /usr/obj${SRCROOT}'
      sh 'umount -f ${SRCROOT}'
      script {
        cleanWs notFailBuild: true
      }
    }
  }
}
