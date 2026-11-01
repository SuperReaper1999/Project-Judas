export default {
  "version": 1,
  "name": "Willow Town resident",
  "interval": 0.3,
  "switchAdvantage": 0.14,
  "actions": [
    {
      "id": "eat", "weight": 1.55, "cooldown": 1, "minCommitment": 2,
      "considerations": [
        {"fact": "hunger", "curve": "power", "exponent": 2},
        {"fact": "distance", "min": 0, "max": 65, "curve": "inverse"}
      ]
    },
    {
      "id": "rest", "weight": 1.45, "cooldown": 1, "minCommitment": 3,
      "considerations": [
        {"fact": "fatigue", "curve": "power", "exponent": 2},
        {"fact": "distance", "min": 0, "max": 65, "curve": "inverse"}
      ]
    },
    {
      "id": "socialize", "weight": 1.25, "cooldown": 2, "minCommitment": 3,
      "considerations": [
        {"fact": "company", "curve": "power", "exponent": 2},
        {"fact": "distance", "min": 0, "max": 65, "curve": "inverse"}
      ]
    },
    {
      "id": "work", "weight": 0.72, "cooldown": 0.6, "minCommitment": 4,
      "considerations": [
        {"fact": "duty"}, {"fact": "readiness"},
        {"fact": "distance", "min": 0, "max": 65, "curve": "inverse"}
      ]
    },
    {
      "id": "jobsearch", "weight": 1.2, "cooldown": 1.5, "minCommitment": 2,
      "considerations": [
        {"fact": "needForWork"},
        {"fact": "distance", "min": 0, "max": 65, "curve": "inverse"}
      ]
    },
    {
      "id": "wander", "weight": 0.085, "cooldown": 1.5, "minCommitment": 2,
      "considerations": [
        {"fact": "distance", "min": 0, "max": 65, "curve": "inverse"}
      ]
    }
  ]
};
