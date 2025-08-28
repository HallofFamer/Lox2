#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "native.h"
#include "object.h"
#include "vm.h"
#include "../common/os.h"

static struct tm dateToTm(int year, int month, int day) {
    struct tm cDate = {
        .tm_year = year - 1900,
        .tm_mon = month - 1,
        .tm_mday = day
    };
    return cDate;
}

static struct tm dateTimeToTm(int year, int month, int day, int hour, int minute, int second) {
    struct tm cDate = {
        .tm_year = year - 1900,
        .tm_mon = month - 1,
        .tm_mday = day,
        .tm_hour = hour,
        .tm_min = minute,
        .tm_sec = second
    };
    return cDate;
}

static double dateGetTimestamp(int year, int month, int day) {
    struct tm cTime = dateToTm(year, month, day);
    return (double)mktime(&cTime);
}

static double dateTimeGetTimestamp(int year, int month, int day, int hour, int minute, int second) {
    struct tm cTime = dateTimeToTm(year, month, day, hour, minute, second);
    return (double)mktime(&cTime);
}

double dateObjGetTimestamp(VM* vm, ObjInstance* date) {
    Value year = getObjField(vm, date, "year");
    Value month = getObjField(vm, date, "month");
    Value day = getObjField(vm, date, "day");
    return dateGetTimestamp(AS_INT(year), AS_INT(month), AS_INT(day));
}

ObjInstance* dateObjNow(VM* vm, ObjClass* klass) {
    time_t nowTime;
    time(&nowTime);
    struct tm now;
    localtime_s(&now, &nowTime);
    ObjInstance* date = newInstance(vm, klass);
    push(vm, OBJ_VAL(date));
    setObjField(vm, date, "year", INT_VAL(1900 + now.tm_year));
    setObjField(vm, date, "month", INT_VAL(1 + now.tm_mon));
    setObjField(vm, date, "day", INT_VAL(now.tm_mday));
    pop(vm);
    return date;
}

double dateTimeObjGetTimestamp(VM* vm, ObjInstance* dateTime) {
    Value year = getObjField(vm, dateTime, "year");
    Value month = getObjField(vm, dateTime, "month");
    Value day = getObjField(vm, dateTime, "day");
    Value hour = getObjField(vm, dateTime, "hour");
    Value minute = getObjField(vm, dateTime, "minute");
    Value second = getObjField(vm, dateTime, "second");
    return dateTimeGetTimestamp(AS_INT(year), AS_INT(month), AS_INT(day), AS_INT(hour), AS_INT(minute), AS_INT(second));
}

ObjInstance* dateObjFromTimestamp(VM* vm, ObjClass* dateClass, double timeValue) {
    time_t timestamp = (time_t)timeValue;
    struct tm time;
    localtime_s(&time, &timestamp);
    ObjInstance* date = newInstance(vm, dateClass);
    push(vm, OBJ_VAL(date));
    setObjField(vm, date, "year", INT_VAL(1900 + time.tm_year));
    setObjField(vm, date, "month", INT_VAL(1 + time.tm_mon));
    setObjField(vm, date, "day", INT_VAL(time.tm_mday));
    pop(vm);
    return date;
}

ObjInstance* dateTimeObjFromTimestamp(VM* vm, ObjClass* dateTimeClass, double timeValue) {
    time_t timestamp = (time_t)timeValue;
    struct tm time;
    localtime_s(&time, &timestamp);
    ObjInstance* dateTime = newInstance(vm, dateTimeClass);
    push(vm, OBJ_VAL(dateTime));
    setObjField(vm, dateTime, "year", INT_VAL(1900 + time.tm_year));
    setObjField(vm, dateTime, "month", INT_VAL(1 + time.tm_mon));
    setObjField(vm, dateTime, "day", INT_VAL(time.tm_mday));
    setObjField(vm, dateTime, "hour", INT_VAL(time.tm_hour));
    setObjField(vm, dateTime, "minute", INT_VAL(time.tm_min));
    setObjField(vm, dateTime, "second", INT_VAL(time.tm_sec));
    pop(vm);
    return dateTime;
}

ObjInstance* dateTimeObjNow(VM* vm, ObjClass* klass) {
    time_t nowTime;
    time(&nowTime);
    struct tm now;
    localtime_s(&now, &nowTime);
    ObjInstance* dateTime = newInstance(vm, getNativeClass(vm, "clox.std.util.DateTime"));
    push(vm, OBJ_VAL(dateTime));
    setObjField(vm, dateTime, "year", INT_VAL(1900 + now.tm_year));
    setObjField(vm, dateTime, "month", INT_VAL(1 + now.tm_mon));
    setObjField(vm, dateTime, "day", INT_VAL(now.tm_mday));
    setObjField(vm, dateTime, "hour", INT_VAL(now.tm_hour));
    setObjField(vm, dateTime, "minute", INT_VAL(now.tm_min));
    setObjField(vm, dateTime, "second", INT_VAL(now.tm_sec));
    pop(vm);
    return dateTime;
}

static void durationInit(int* duration, int days, int hours, int minutes, int seconds) {
    if (seconds > 60) {
        minutes += seconds / 60;
        seconds %= 60;
    }

    if (minutes > 60) {
        hours += minutes / 60;
        minutes %= 60;
    }

    if (hours > 60) {
        days += hours / 24;
        hours %= 24;
    }

    duration[0] = days;
    duration[1] = hours;
    duration[2] = minutes;
    duration[3] = seconds;
}

void durationFromSeconds(int* duration, double seconds) {
    durationInit(duration, 0, 0, 0, (int)seconds);
}

void durationFromArgs(int* duration, Value* args) {
    durationInit(duration, AS_INT(args[0]), AS_INT(args[1]), AS_INT(args[2]), AS_INT(args[3]));
}

void durationObjInit(VM* vm, int* duration, ObjInstance* object) {
    push(vm, OBJ_VAL(object));
    setObjField(vm, object, "days", INT_VAL(duration[0]));
    setObjField(vm, object, "hours", INT_VAL(duration[1]));
    setObjField(vm, object, "minutes", INT_VAL(duration[2]));
    setObjField(vm, object, "seconds", INT_VAL(duration[3]));
    pop(vm);
}

double durationTotalSeconds(VM* vm, ObjInstance* duration) {
    Value days = getObjField(vm, duration, "days");
    Value hours = getObjField(vm, duration, "hours");
    Value minutes = getObjField(vm, duration, "minutes");
    Value seconds = getObjField(vm, duration, "seconds");
    return 86400.0 * AS_INT(days) + 3600.0 * AS_INT(hours) + 60.0 * AS_INT(minutes) + AS_INT(seconds);
}