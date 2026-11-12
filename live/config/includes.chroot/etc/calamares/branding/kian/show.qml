import QtQuick 2.0;
import calamares.slideshow 1.0;

Presentation {
    id: presentation

    Slide {
        Text {
            id: text1
            anchors.centerIn: parent
            horizontalAlignment: Text.AlignHCenter
            text: "<h1>Welcome to Kian OS 1.0</h1><br/>An Ubuntu 24.04 LTS base,<br/>the Kian desktop, and a real installer.<br/><br/>Your files will really be yours."
            wrap: true
            width: parent.width - 100
            font.pointSize: 14
            color: "#1A2B4A"
        }
    }
}
