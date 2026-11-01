export default {
  "version": 1,
  "name": "Citizen competing needs",
  "interval": 0.2,
  "switchAdvantage": 0.15,
  "actions": [
    {
      "id": "eat",
      "weight": 1.4,
      "mode": "product",
      "cooldown": 0.4,
      "minCommitment": 1.2,
      "interruptible": true,
      "requiredFacts": [],
      "considerations": [
        {
          "fact": "hunger",
          "min": 0,
          "max": 1,
          "curve": "linear",
          "weight": 1
        },
        {
          "fact": "distance",
          "min": 0,
          "max": 25,
          "curve": "inverse",
          "weight": 1
        }
      ]
    },
    {
      "id": "rest",
      "weight": 1.25,
      "mode": "product",
      "cooldown": 0.4,
      "minCommitment": 1.2,
      "interruptible": true,
      "requiredFacts": [],
      "considerations": [
        {
          "fact": "fatigue",
          "min": 0,
          "max": 1,
          "curve": "linear",
          "weight": 1
        },
        {
          "fact": "distance",
          "min": 0,
          "max": 25,
          "curve": "inverse",
          "weight": 1
        }
      ]
    },
    {
      "id": "socialize",
      "weight": 1.1,
      "mode": "product",
      "cooldown": 0.4,
      "minCommitment": 1.2,
      "interruptible": true,
      "requiredFacts": [],
      "considerations": [
        {
          "fact": "company",
          "min": 0,
          "max": 1,
          "curve": "linear",
          "weight": 1
        },
        {
          "fact": "distance",
          "min": 0,
          "max": 25,
          "curve": "inverse",
          "weight": 1
        }
      ]
    },
    {
      "id": "wander",
      "weight": 0.12,
      "mode": "product",
      "cooldown": 0.4,
      "minCommitment": 1.2,
      "interruptible": true,
      "requiredFacts": [],
      "considerations": [
        {
          "fact": "distance",
          "min": 0,
          "max": 25,
          "curve": "inverse",
          "weight": 1
        }
      ]
    }
  ]
};
