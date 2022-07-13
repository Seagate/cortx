FROM adoptopenjdk/openjdk11:latest
ARG JAR_FILE=build/libs/cortx-drive-1.0.0-SNAPSHOT.jar
COPY ${JAR_FILE} app.jar
ENTRYPOINT ["java","-jar","/app.jar"]