"""Keep local and CI development builds compatible with the published APK."""
import base64
import hashlib
from pathlib import Path
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[2]
KEYSTORE = ROOT / 'app/falloutquest-dev.keystore'
# Certificate extracted from the published v376 APK signing block.
PUBLISHED_CERTIFICATE = '4e98fe54c4560cbadc86ac1863197ba1e2b2340a233c937cd96271a0cc8a1904'


class SigningKeyTests(unittest.TestCase):
    def test_ci_and_local_keystores_match(self):
        encoded = KEYSTORE.with_suffix('.keystore.b64').read_bytes()
        self.assertEqual(base64.b64decode(encoded), KEYSTORE.read_bytes())

    def test_certificate_matches_published_apk(self):
        certificate = subprocess.check_output([
            'keytool', '-exportcert', '-keystore', str(KEYSTORE),
            '-storepass', 'falloutquest-dev', '-alias', 'falloutquest-dev',
        ], stderr=subprocess.PIPE)
        self.assertEqual(hashlib.sha256(certificate).hexdigest(), PUBLISHED_CERTIFICATE)


if __name__ == '__main__':
    unittest.main()
