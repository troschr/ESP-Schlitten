#include "comm/CommandInterface.h"

namespace esp_schlitten {

void CommandInterface::begin(Stream &serial) {
  serial_ = &serial;
  inputBuffer_.reserve(kMaxLineLength);
}

void CommandInterface::poll() {
  if (serial_ == nullptr) {
    return;
  }

  while (serial_->available() > 0) {
    const char next = static_cast<char>(serial_->read());

    if (next == '\r' || next == '\n') {
      if (inputBuffer_.length() > 0) {
        const Command command = parseLine(inputBuffer_);
        if (!enqueue(command)) {
          sendResponseError(command.id, ErrorCode::Busy);
        }
        inputBuffer_ = "";
      }
      continue;
    }

    if (inputBuffer_.length() >= kMaxLineLength) {
      inputBuffer_ = "";
      sendResponseError(0, ErrorCode::InvalidCommand);
      continue;
    }

    inputBuffer_ += next;
  }
}

bool CommandInterface::hasPendingCommand() const {
  return count_ > 0;
}

Command CommandInterface::popCommand() {
  if (count_ == 0) {
    return {};
  }

  const Command command = queue_[head_];
  head_ = static_cast<uint8_t>((head_ + 1U) % kQueueCapacity);
  --count_;
  return command;
}

void CommandInterface::sendLine(const String &line) {
  if (serial_ == nullptr) {
    return;
  }

  serial_->println(line);
}

void CommandInterface::sendAck(uint32_t id) {
  sendLine(String("RSP;") + String(id) + ";ACK");
}

void CommandInterface::sendResponseError(uint32_t id, ErrorCode error) {
  sendLine(String("RSP;") + String(id) + ";ERR;" + String(toString(error)));
}

Command CommandInterface::parseLine(String line) const {
  line.trim();

  Command command;
  if (line.length() == 0) {
    command.parseError = ErrorCode::InvalidCommand;
    return command;
  }

  String fields[8];
  uint8_t fieldCount = 0;
  int startIndex = 0;

  while (startIndex <= line.length() && fieldCount < 8) {
    const int delimiterIndex = line.indexOf(';', startIndex);
    if (delimiterIndex < 0) {
      fields[fieldCount++] = line.substring(startIndex);
      break;
    }

    fields[fieldCount++] = line.substring(startIndex, delimiterIndex);
    startIndex = delimiterIndex + 1;
  }

  if (fieldCount < 3) {
    command.parseError = ErrorCode::InvalidCommand;
    return command;
  }

  fields[0].trim();
  if (!fields[0].equalsIgnoreCase("CMD")) {
    command.parseError = ErrorCode::InvalidCommand;
    return command;
  }

  fields[1].trim();
  command.id = static_cast<uint32_t>(fields[1].toInt());

  fields[2].trim();
  fields[2].toUpperCase();

  if (fields[2] == "STATUS") {
    command.type = CommandType::Status;
    command.valid = true;
    return command;
  }

  if (fields[2] == "STOP") {
    command.type = CommandType::Stop;
    command.valid = true;
    return command;
  }

  if (fields[2] == "HOME") {
    command.type = CommandType::Home;
    command.valid = true;
    return command;
  }

  if (fields[2] == "RESET_ERROR") {
    command.type = CommandType::ResetError;
    command.valid = true;
    return command;
  }

  if (fields[2] == "PING") {
    command.type = CommandType::Ping;
    command.valid = true;
    return command;
  }

  if (fields[2] == "MOVE_TO") {
    command.type = CommandType::MoveTo;
    command.speedStepsPerSecond = 0;
    command.accelerationStepsPerSecond2 = 0;

    for (uint8_t i = 3; i < fieldCount; ++i) {
      String key;
      String value;
      if (!parseKeyValue(fields[i], key, value)) {
        command.parseError = ErrorCode::InvalidCommand;
        return command;
      }

      key.toLowerCase();
      value.trim();

      if (key == "axis") {
        value.toUpperCase();
        if (value != "Z") {
          command.parseError = ErrorCode::InvalidCommand;
          return command;
        }
      } else if (key == "pos") {
        command.positionSteps = value.toInt();
      } else if (key == "speed") {
        command.speedStepsPerSecond = static_cast<uint32_t>(value.toInt());
      } else if (key == "acc") {
        command.accelerationStepsPerSecond2 = static_cast<uint32_t>(value.toInt());
      } else {
        command.parseError = ErrorCode::InvalidCommand;
        return command;
      }
    }

    command.valid = true;
    return command;
  }

  if (fields[2] == "SET_SERVO") {
    command.type = CommandType::SetServo;

    for (uint8_t i = 3; i < fieldCount; ++i) {
      String key;
      String value;
      if (!parseKeyValue(fields[i], key, value)) {
        command.parseError = ErrorCode::InvalidCommand;
        return command;
      }

      key.toLowerCase();
      value.toUpperCase();

      if (key != "position") {
        command.parseError = ErrorCode::InvalidCommand;
        return command;
      }

      if (value == "OPEN") {
        command.holderPosition = HolderPosition::Open;
      } else if (value == "CLOSED") {
        command.holderPosition = HolderPosition::Closed;
      } else if (value == "SERVICE") {
        command.holderPosition = HolderPosition::Service;
      } else {
        command.parseError = ErrorCode::InvalidCommand;
        return command;
      }
    }

    if (command.holderPosition == HolderPosition::Unknown) {
      command.parseError = ErrorCode::InvalidCommand;
      return command;
    }

    command.valid = true;
    return command;
  }

  command.parseError = ErrorCode::InvalidCommand;
  return command;
}

bool CommandInterface::enqueue(const Command &command) {
  if (count_ >= kQueueCapacity) {
    return false;
  }

  queue_[tail_] = command;
  tail_ = static_cast<uint8_t>((tail_ + 1U) % kQueueCapacity);
  ++count_;
  return true;
}

bool CommandInterface::parseKeyValue(const String &fragment, String &key, String &value) const {
  const int separatorIndex = fragment.indexOf('=');
  if (separatorIndex < 1 || separatorIndex >= fragment.length() - 1) {
    return false;
  }

  key = fragment.substring(0, separatorIndex);
  value = fragment.substring(separatorIndex + 1);
  key.trim();
  value.trim();
  return key.length() > 0 && value.length() > 0;
}

}  // namespace esp_schlitten
