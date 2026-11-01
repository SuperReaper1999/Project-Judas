export default {
  "version": 1,
  "name": "Guard self preservation",
  "interval": 0.15,
  "switchAdvantage": 0.12,
  "actions": [
    {
      "id": "hide",
      "weight": 4,
      "mode": "product",
      "cooldown": 0.4,
      "minCommitment": 1.2,
      "interruptible": true,
      "requiredFacts": [],
      "considerations": [
        {
          "fact": "health",
          "min": 0,
          "max": 1,
          "curve": "inverse",
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
      "id": "recover",
      "weight": 4,
      "mode": "product",
      "cooldown": 0.4,
      "minCommitment": 1.2,
      "interruptible": true,
      "requiredFacts": [],
      "considerations": [
        {
          "fact": "health",
          "min": 0,
          "max": 1,
          "curve": "inverse",
          "weight": 1
        }
      ]
    },
    {
      "id": "patrol",
      "weight": 0.2,
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
