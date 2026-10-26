const unsigned int TRAINER_SPEEDUP_STREAK = 10;
const char* const TRAINER_WORDS[] = {
  "CQ", "DE", "TEST", "SOS", "QTH", "NAME", "RST", "QSL",
  "73", "599", "DL1ABC", "W1AW", "G4ABC", "JA1XYZ", "F5ABC"
};

void StartTrainerSession()
{
  if (!trainerSessionActive)
  {
    trainerPreviousWpm = wpm;
    trainerPreviousFwpm = fwpm;
  }

  trainerSessionActive = true;
  wpm = trainerStartWpm;
  fwpm = trainerStartWpm;
  CalculateTimes(wpm, fwpm);

  trainerSessionCorrect = 0;
  trainerSessionAttempts = 0;
  trainerSessionSkipped = 0;
  trainerCorrectStreak = 0;
  trainerRetryQueue = "";
  trainerPrompt = "";
  trainerResponse = "";
  trainerUsingGroup = false;
  trainerAdvancePending = false;
  trainerSpeedIncreased = false;
}

void EndTrainerSession()
{
  if (!trainerSessionActive)
    return;

  StopTrainerQuestion();
  wpm = trainerPreviousWpm;
  fwpm = trainerPreviousFwpm;
  CalculateTimes(wpm, fwpm);
  trainerSessionActive = false;
}

char SelectTrainerCharacter()
{
  if (selectedLetters.length() == 0)
    selectedLetters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

  if (trainerRetryQueue.length() > 0 && random(0, 100) < 70)
  {
    String eligibleMisses = "";
    for (unsigned int i = 0; i < trainerRetryQueue.length(); i++)
    {
      if (selectedLetters.indexOf(trainerRetryQueue[i]) != -1)
        eligibleMisses += trainerRetryQueue[i];
    }
    if (eligibleMisses.length() > 0)
      return eligibleMisses[random(0, eligibleMisses.length())];
  }

  return selectedLetters[random(0, selectedLetters.length())];
}

String SelectTrainerWord()
{
  if (selectedLetters.length() == 0)
    selectedLetters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  trainerUsingGroup = false;

  byte candidates[sizeof(TRAINER_WORDS) / sizeof(TRAINER_WORDS[0])];
  byte candidateCount = 0;
  byte retryCandidates[sizeof(TRAINER_WORDS) / sizeof(TRAINER_WORDS[0])];
  byte retryCandidateCount = 0;

  for (byte i = 0; i < sizeof(TRAINER_WORDS) / sizeof(TRAINER_WORDS[0]); i++)
  {
    const char* word = TRAINER_WORDS[i];
    bool available = true;
    bool needsPractice = false;

    for (unsigned int j = 0; word[j] != '\0'; j++)
    {
      if (selectedLetters.indexOf(word[j]) == -1)
      {
        available = false;
        break;
      }
      if (trainerRetryQueue.indexOf(word[j]) != -1)
        needsPractice = true;
    }

    if (available)
    {
      candidates[candidateCount++] = i;
      if (needsPractice)
        retryCandidates[retryCandidateCount++] = i;
    }
  }

  if (retryCandidateCount > 0 && random(0, 100) < 70)
    return String(TRAINER_WORDS[retryCandidates[random(0, retryCandidateCount)]]);
  if (candidateCount > 0)
    return String(TRAINER_WORDS[candidates[random(0, candidateCount)]]);

  trainerUsingGroup = true;
  String group = "";
  for (byte i = 0; i < 4; i++)
    group += SelectTrainerCharacter();
  return group;
}

void ShowTrainerProgress()
{
  unsigned int accuracy = 0;
  if (trainerSessionAttempts > 0)
    accuracy = (trainerSessionCorrect * 100UL) / trainerSessionAttempts;

  char progress[17];
  snprintf(progress, sizeof(progress), "%lu/%lu %u%% S%lu",
           trainerSessionCorrect, trainerSessionAttempts, accuracy,
           trainerSessionSkipped);
  display.print("                ", 7, 0);
  display.print(progress, 7, 0);
}

void RecordTrainerAnswer(bool correct)
{
  trainerSpeedIncreased = false;
  trainerSessionAttempts++;

  if (correct)
  {
    trainerSessionCorrect++;
    trainerCorrectStreak++;

    for (unsigned int i = 0; i < trainerPrompt.length(); i++)
    {
      int retryIndex = trainerRetryQueue.indexOf(trainerPrompt[i]);
      if (retryIndex >= 0)
        trainerRetryQueue.remove(retryIndex, 1);
    }

    if (trainerCorrectStreak % TRAINER_SPEEDUP_STREAK == 0 &&
        wpm <= TRAINER_MAX_WPM - TRAINER_WPM_STEP)
    {
      trainerSpeedIncreased = true;
      wpm += TRAINER_WPM_STEP;
      fwpm = wpm;
      CalculateTimes(wpm, fwpm);
    }
  }
  else
  {
    trainerCorrectStreak = 0;
    if (trainerPrompt.length() > trainerResponse.length())
    {
      char missed = trainerPrompt[trainerResponse.length()];
      if (trainerRetryQueue.length() >= 32)
        trainerRetryQueue.remove(0, 1);
      trainerRetryQueue += missed;
    }
  }

  ShowTrainerProgress();
}

void ShowTrainerFeedback(bool correct)
{
  display.print("                ", 5, 0);
  if (correct)
  {
    display.print(trainerSpeedIncreased ? "Level up!" : "Correct!", 5, 4);
    if (trainerSpeedIncreased)
    {
      char speed[17];
      snprintf(speed, sizeof(speed), "Now %d WPM", wpm);
      display.print(speed, 6, 3);
    }
  }
  else
  {
    display.print("Answer:", 5, 1);
    display.print(trainerPrompt.c_str(), 6, 1);
  }
  ShowTrainerProgress();
}

void SubmitTrainerCharacter(char answer)
{
  bool visiblePrompt = actual_menu == TRAINER_GIVE_RANDOM_SCREEN ||
                       actual_menu == TRAINER_GIVE_AZ_SCREEN;

  if (visiblePrompt)
  {
    trainerPrompt = String(currentTrainerLetter);
    trainerResponse = "";
    bool correct = answer == currentTrainerLetter;
    RecordTrainerAnswer(correct);
    ShowTrainerFeedback(correct);
    if (!correct)
      ProcessSign(currentTrainerLetter);
    trainerAdvancePending = true;
    trainerAdvanceAt = millis() + 1000;
    return;
  }

  if (trainerResponse.length() >= trainerPrompt.length())
    return;

  char expected = trainerPrompt[trainerResponse.length()];
  if (answer != expected)
  {
    RecordTrainerAnswer(false);
    ShowTrainerFeedback(false);
    startPlayback(trainerPrompt);
    trainerAdvancePending = true;
    trainerAdvanceAt = millis() + 1000;
    return;
  }

  trainerResponse += answer;
  if (trainerResponse.length() == trainerPrompt.length())
  {
    RecordTrainerAnswer(true);
    ShowTrainerFeedback(true);
    trainerAdvancePending = true;
    trainerAdvanceAt = millis() + 1000;
    return;
  }

  char progress[17];
  snprintf(progress, sizeof(progress), "Correct %u/%u",
           trainerResponse.length(), trainerPrompt.length());
  display.print("                ", 5, 0);
  display.print(progress, 5, 2);
}

void StartNextTrainerQuestion()
{
  trainerAdvancePending = false;

  if (actual_menu == TRAINER_GIVE_RANDOM_SCREEN)
    ShowTrainerGiveRandomScreen();
  else if (actual_menu == TRAINER_GIVE_AZ_SCREEN)
    ShowTrainerGiveAZScreen();
  else if (actual_menu == TRAINER_HEAR_SCREEN)
    ShowTrainerHearScreen();
  else if (actual_menu == TRAINER_LISTEN_REPEAT_SCREEN)
    ShowTrainerListenRepeatScreen();
}

bool IsTrainerQuestionActive()
{
  return actual_menu == TRAINER_GIVE_RANDOM_SCREEN ||
         actual_menu == TRAINER_GIVE_AZ_SCREEN ||
         actual_menu == TRAINER_HEAR_SCREEN ||
         actual_menu == TRAINER_LISTEN_REPEAT_SCREEN;
}

void ReplayTrainerQuestion()
{
  if (!IsTrainerQuestionActive() || trainerAdvancePending)
    return;

  decoderString = "";
  trainerResponse = "";

  if (actual_menu == TRAINER_GIVE_RANDOM_SCREEN ||
      actual_menu == TRAINER_GIVE_AZ_SCREEN)
  {
    display.clear();
    display.print(actual_menu == TRAINER_GIVE_RANDOM_SCREEN ? "Give Random" : "Give A-Z",
                  0, actual_menu == TRAINER_GIVE_RANDOM_SCREEN ? 2 : 4);
    display.print("Tap=again", 1, 3);
    display.printChar16(currentTrainerLetter, 3, 7);
    display.print("Turn=skip", 5, 3);
    display.print("Hold=menu", 6, 3);
    ShowTrainerProgress();
    return;
  }

  playing = false;
  beeping = false;
  inPause = false;
  noTone(SPEAKER_PIN);
  digitalWrite(BUZZER_PIN, LOW);

  display.clear();
  if (actual_menu == TRAINER_HEAR_SCREEN)
  {
    display.print("Listen Char", 0, 2);
    display.print("Tap=replay", 1, 3);
    display.print("Listen & key", 2, 2);
    display.print("Turn=skip", 3, 3);
    display.print("?", 4, 7);
    display.print("Hold=menu", 6, 3);
  }
  else
  {
    display.print(trainerUsingGroup ? "Copy 4-Ch Group" : "Copy Word/Call", 0, 1);
    display.print("Tap=replay", 1, 3);
    display.print("Listen, then key", 2, 0);
    display.print("Turn=skip", 3, 3);
    display.print("Hold=menu", 6, 3);
  }
  ShowTrainerProgress();
  startPlayback(trainerPrompt);
}

void SkipTrainerQuestion()
{
  if (!IsTrainerQuestionActive() || trainerAdvancePending)
    return;

  char missed;
  if (actual_menu == TRAINER_GIVE_RANDOM_SCREEN ||
      actual_menu == TRAINER_GIVE_AZ_SCREEN)
    missed = currentTrainerLetter;
  else if (trainerResponse.length() < trainerPrompt.length())
    missed = trainerPrompt[trainerResponse.length()];
  else
    return;

  if (trainerRetryQueue.length() >= 32)
    trainerRetryQueue.remove(0, 1);
  trainerRetryQueue += missed;
  trainerCorrectStreak = 0;
  trainerSessionSkipped++;

  display.print("Skipped", 5, 4);
  if (trainerPrompt.length() > 0)
    display.print(trainerPrompt.c_str(), 6, 1);
  else
  {
    char answer[2] = {missed, '\0'};
    display.print(answer, 6, 1);
  }
  ShowTrainerProgress();
  if (trainerPrompt.length() > 0)
    startPlayback(trainerPrompt);
  else
    ProcessSign(missed);
  trainerAdvancePending = true;
  trainerAdvanceAt = millis() + 1000;
}

void StopTrainerQuestion()
{
  if (actual_menu == TRAINER_GIVE_AZ_SCREEN)
    selectedLetters = ReadTextFromEEPROM(0x40);

  trainerAdvancePending = false;
  playing = false;
  beeping = false;
  inPause = false;
  trainerPrompt = "";
  trainerResponse = "";
  decoderString = "";
  noTone(SPEAKER_PIN);
  digitalWrite(BUZZER_PIN, LOW);
}
