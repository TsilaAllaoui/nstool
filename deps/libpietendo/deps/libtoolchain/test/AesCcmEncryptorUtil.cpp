#include "AesCcmEncryptorUtil.h"

#include <tc/cli/FormatUtil.h>
#include <fmt/format.h>

void AesCcmEncryptorUtil::generateAesCcmTestVectors_Nist(std::vector<AesCcmEncryptorUtil::TestVector>& test_list, size_t key_size)
{
	AesCcmEncryptorUtil::TestVector tmp;

	if (key_size == 128)
	{
		// Clear test cases
		test_list.clear();

		/*
		* Nintendo 3DS InitialData
		*/
		{
			tmp.test_name = "N3DS InitialData (DevCCI)";
			tmp.key = tc::ByteData(16);
			tmp.nonce = tc::ByteData(12);
			tmp.aad = tc::ByteData();
			tmp.mac = tc::ByteData({0xAD, 0x88, 0xAC, 0x41, 0xA2, 0xB1, 0x5E, 0x8F, 0x66, 0x9C, 0x97, 0xE5, 0xE1, 0x5E, 0xA3, 0xEB});
			tmp.plaintext = tc::ByteData({0x6E, 0xC7, 0x5F, 0xB2, 0xE2, 0xB4, 0x87, 0x46, 0x1E, 0xDD, 0xCB, 0xB8, 0x97, 0x11, 0x92, 0xBA});
			tmp.ciphertext = tc::ByteData(16);
			test_list.push_back(tmp);
		}

		/*
		* Examples 1 to 3 taken from NIST SP 800-38C Appendix C
		*/
		{
			tc::ByteData base_key = tc::ByteData({0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f});
			tc::ByteData base_iv = tc::ByteData({0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b});
			tc::ByteData base_add = tc::ByteData({0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13});
			tc::ByteData base_msg = tc::ByteData({0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37});

			static const size_t kTestNum = 3;

			static const size_t test_iv_size [kTestNum] = { 7, 8,  12 };
			static const size_t test_add_size[kTestNum] = { 8, 16, 20 };
			static const size_t test_msg_size[kTestNum] = { 4, 16, 24 };

			std::array<tc::ByteData, kTestNum> expected_mac = {
				tc::ByteData({0x4d, 0xac, 0x25, 0x5d}),
				tc::ByteData({0x1f, 0xc6, 0x4f, 0xbf, 0xac, 0xcd}),
				tc::ByteData({0x48, 0x43, 0x92, 0xfb, 0xc1, 0xb0, 0x99, 0x51})
			};
			std::array<tc::ByteData, kTestNum> expected_ciphertext = {
				tc::ByteData({0x71, 0x62, 0x01, 0x5b}),
				tc::ByteData({0xd2, 0xa1, 0xf0, 0xe0, 0x51, 0xea, 0x5f, 0x62, 0x08, 0x1a, 0x77, 0x92, 0x07, 0x3d, 0x59, 0x3d}),
				tc::ByteData({0xe3, 0xb2, 0x01, 0xa9, 0xf5, 0xb7, 0x1a, 0x7a, 0x9b, 0x1c, 0xea, 0xec, 0xcd, 0x97, 0xe7, 0x0b, 0x61, 0x76, 0xaa, 0xd9, 0xa4, 0x42, 0x8a, 0xa5})
			};

			for (size_t i = 0; i < kTestNum; i++)
			{
				tmp.test_name = fmt::format("SP800-38C Appendix C, Example {:d}", i);

				tmp.key = base_key;
				tmp.nonce = tc::ByteData(base_iv.data(), test_iv_size[i]);
				tmp.aad = tc::ByteData(base_add.data(), test_add_size[i]);
				tmp.mac = expected_mac[i];
				tmp.plaintext = tc::ByteData(base_msg.data(), test_msg_size[i]);
				tmp.ciphertext = expected_ciphertext[i];
				test_list.push_back(tmp);
			}
		}
	}
}