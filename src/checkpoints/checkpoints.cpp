// Parts are Copyright (c) 2019, The Dinastycoin team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Parts of this file are originally copyright (c) 2015-2019 The Monero Project

#include "checkpoints.h"

#include "common/dns_utils.h"
#include "string_tools.h"
#include "storages/portable_storage_template_helper.h" // epee json include
#include "serialization/keyvalue_serialization.h"
#include <functional>
#include <vector>
#include <boost/bind.hpp>

using namespace epee;

#undef DINASTYCOIN_DEFAULT_LOG_CATEGORY
#define DINASTYCOIN_DEFAULT_LOG_CATEGORY "checkpoints"

namespace cryptonote
{
  /**
   * @brief struct for loading a checkpoint from json
   */
  struct t_hashline
  {
    uint64_t height; //!< the height of the checkpoint
    std::string hash; //!< the hash for the checkpoint
        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(height)
          KV_SERIALIZE(hash)
        END_KV_SERIALIZE_MAP()
  };

  /**
   * @brief struct for loading many checkpoints from json
   */
  struct t_hash_json {
    std::vector<t_hashline> hashlines; //!< the checkpoint lines from the file
        BEGIN_KV_SERIALIZE_MAP()
          KV_SERIALIZE(hashlines)
        END_KV_SERIALIZE_MAP()
  };

  //---------------------------------------------------------------------------
  checkpoints::checkpoints()
  {
  }
  //---------------------------------------------------------------------------
  bool checkpoints::add_checkpoint(uint64_t height, const std::string& hash_str, const std::string& difficulty_str)
  {
    crypto::hash h = crypto::null_hash;
    bool r = epee::string_tools::hex_to_pod(hash_str, h);
    CHECK_AND_ASSERT_MES(r, false, "Failed to parse checkpoint hash string into binary representation!");

    // return false if adding at a height we already have AND the hash is different
    if (m_points.count(height))
    {
      CHECK_AND_ASSERT_MES(h == m_points[height], false, "Checkpoint at given height already exists, and hash for new checkpoint was different!");
    }
    m_points[height] = h;
    if (!difficulty_str.empty())
    {
      try
      {
        difficulty_type difficulty(difficulty_str);
        if (m_difficulty_points.count(height))
        {
          CHECK_AND_ASSERT_MES(difficulty == m_difficulty_points[height], false, "Difficulty checkpoint at given height already exists, and difficulty for new checkpoint was different!");
        }
        m_difficulty_points[height] = difficulty;
      }
      catch (...)
      {
        LOG_ERROR("Failed to parse difficulty checkpoint: " << difficulty_str);
        return false;
      }
    }
    return true;
  }
  //---------------------------------------------------------------------------
  bool checkpoints::is_in_checkpoint_zone(uint64_t height) const
  {
    return !m_points.empty() && (height <= (--m_points.end())->first);
  }
  //---------------------------------------------------------------------------
  bool checkpoints::check_block(uint64_t height, const crypto::hash& h, bool& is_a_checkpoint) const
  {
    auto it = m_points.find(height);
    is_a_checkpoint = it != m_points.end();
    if(!is_a_checkpoint)
      return true;

    if(it->second == h)
    {
      MINFO("CHECKPOINT PASSED FOR HEIGHT " << height << " " << h);
      return true;
    }else
    {
      MWARNING("CHECKPOINT FAILED FOR HEIGHT " << height << ". EXPECTED HASH: " << it->second << ", FETCHED HASH: " << h);
      return false;
    }
  }
  //---------------------------------------------------------------------------
  bool checkpoints::check_block(uint64_t height, const crypto::hash& h) const
  {
    bool ignored;
    return check_block(height, h, ignored);
  }
  //---------------------------------------------------------------------------
  //FIXME: is this the desired behavior?
  bool checkpoints::is_alternative_block_allowed(uint64_t blockchain_height, uint64_t block_height) const
  {
    if (0 == block_height)
      return false;

    auto it = m_points.upper_bound(blockchain_height);
    // Is blockchain_height before the first checkpoint?
    if (it == m_points.begin())
      return true;

    --it;
    uint64_t checkpoint_height = it->first;
    return checkpoint_height < block_height;
  }
  //---------------------------------------------------------------------------
  uint64_t checkpoints::get_max_height() const
  {
    if (m_points.empty())
      return 0;
    return m_points.rbegin()->first;
  }
  //---------------------------------------------------------------------------
  const std::map<uint64_t, crypto::hash>& checkpoints::get_points() const
  {
    return m_points;
  }
  //---------------------------------------------------------------------------
  const std::map<uint64_t, difficulty_type>& checkpoints::get_difficulty_points() const
  {
    return m_difficulty_points;
  }

  bool checkpoints::check_for_conflicts(const checkpoints& other) const
  {
    for (auto& pt : other.get_points())
    {
      if (m_points.count(pt.first))
      {
        CHECK_AND_ASSERT_MES(pt.second == m_points.at(pt.first), false, "Checkpoint at given height already exists, and hash for new checkpoint was different!");
      }
    }
    return true;
  }

  bool checkpoints::init_default_checkpoints(network_type nettype)
  {
    if (nettype == TESTNET)
    {

      return true;
    }
    if (nettype == STAGENET)
    {

      return true;
    }
    ADD_CHECKPOINT(1,    "7b9b6064c13231bda96759fcabd21255af66f94ddece53695509ecb528479667");
    ADD_CHECKPOINT(10 , "7fff6b1b180abe1ade902232b0d39372dd165e82addd0a12514b69115ab29789");
    ADD_CHECKPOINT(100, "b922e51c7cccba7f7fd12b395b942a6092566c47879862b127405dc16c3b415a");
    ADD_CHECKPOINT(500, "4161494672a7ef39e1a1c6d5e4b3c6e899b5a945cd1dd7239ad734189c663f29");
    ADD_CHECKPOINT(1000, "f75b44cbf1f070814ae83bb54d0d0b98ee0583633ed88b21088a3957ccb675c0");
    ADD_CHECKPOINT(2000, "a739216d63de35fa69c74ff22c2ed201fd2d0dbe7c38a8bbdbb64368fd18aff1");
    ADD_CHECKPOINT(3000, "0d5882e703a4e715450cc2538ead37d2ad2960c0ad9245546187c04b11ae5b4c");
    ADD_CHECKPOINT(4000, "d66aee31dff6b06f5d6f56fdaab71247325b818968c3c555f6626969965487eb");
    ADD_CHECKPOINT(5000, "458bf83117978a24c16e77419d450e81dc808ed8288e3ff301f3e9ff41520b0a");
    ADD_CHECKPOINT(5353, "e96ad3449cec0f97978f1c79120d713c1753116d778b33c6d5609bed99fdd2a7");
    ADD_CHECKPOINT(5500, "58cea8b62686f3a3c0c8f9edd30b02810cad1033ad2eea05fe47f63f0838a460");
    ADD_CHECKPOINT(5544, "963e97cad472b7ab43676129d7eb87c0791ee0f160634ea7d26b02f29230c740");
    ADD_CHECKPOINT(6000, "50f4c25ab0997c79f47b32aa7a766a3821e5e40935d46e03260ca1a913138df1");
    ADD_CHECKPOINT(6500, "f26226611fcd1437882f1a3a484cc8823ea59d009cace890620c093b587b4487"); 
    ADD_CHECKPOINT(7000, "522b3f918a3976bf79b4802aba906c318880d73daef5e8a3d168b59096a43f3c");
    ADD_CHECKPOINT(8000, "ee949fccb6f4db661f5a38e4c8f487dbaf5bd18bacfb4d77b32eb3bc3abb7794");
    ADD_CHECKPOINT(9500, "b62d0dae7be7012138af83244160797389fffb3ef2aae2ec3d91082b1a58a047");
    ADD_CHECKPOINT(10000,"92388506769d6ee510af6f480099a1f5466a6cae855bb5c51e0bb328457cd5d4"); 
    ADD_CHECKPOINT(12000,"63554dd0ae6f178f5a8bb94232e5004cae09d3d797d0953c48d0cd93b6b3743c");
    ADD_CHECKPOINT(15622,"189a796e8fb84bdcca69cf8dc2336f0d652a11504dc9c8b5da7f217ae331e867");
    ADD_CHECKPOINT(20000, "5507b571ba1f634810627ca2a8450b894d474762cffd79ddbfaefee3b96f22a5");
    ADD_CHECKPOINT(32139,"b6bb051810a65fdf20c12b8b847e306e670861abeecbfb126b7eb3be55f559ac");
    ADD_CHECKPOINT(39638, "e8d7e2d5389ed04e6beaa53dbc6707a47e76d8f86f074a434ff2e4ff74cda5f3");
    ADD_CHECKPOINT(226000, "d4e076d8a4c23e6e51df50ae038f710fe83b1363c69b5d6c94c3d227912ff10c");
    ADD_CHECKPOINT(263664, "3ea3ebf33bc4c73b00d28addabdf47ca2bf9b0a202f2646a01f5a9121e5d3a54");
    ADD_CHECKPOINT(300000, "8c5a9f86b20861c1dee6ab90ac86d0b1816163c11f5cf8e23566157e36043998") ;
    ADD_CHECKPOINT(325000, "bc68a60c42480c04877d97611a6d161bf53d5a6c6460073830b32db8bd50d1f0") ;
    ADD_CHECKPOINT(333000, "b4ef852358df0ff76ed878704b823c82dc8799a83a3caa10874cf56a957b54d8") ;	
    ADD_CHECKPOINT(400000, "5ea6a74691c402be4f428954c00c9b9359a9a1f9afac1317e8115cf793efa039") ;
    ADD_CHECKPOINT(410500, "7df88b7fd6db121d47571afbbee4cc83a32619d96588eb0de3b082c96b0aa163") ;
    ADD_CHECKPOINT(425500, "a4e58148141c3389c9a6f25b100fa2c9db2528a83823bfd3cdf343a901fa509b") ;
    ADD_CHECKPOINT(435000, "b224c2aa24abd65984003200516d3a7a6be511c4f858151e613a4d640dafec75") ;
    ADD_CHECKPOINT(450555, "ffac1a65eb6e9d1e240d162a2f7a67f7a4a35a70548abd5e62dfd11cd378469a") ;
    ADD_CHECKPOINT(465000, "0a4a9f3ed25ef43f85b95c34b1d11cdd895da8f90574b555a04a112e0f0004c2") ;
    ADD_CHECKPOINT(469666, "b5381cf17128d24ce5f7468e3bcbc79c9b227facf13015dbfa825fd67e6ec026") ;
    ADD_CHECKPOINT(475000, "a3c90ed7101d21fbe0b5fb8e7477ca71bcb165f4de7406ab400b990b334199f0") ;
    ADD_CHECKPOINT(476483, "8d0f2022420c2d5dc0c3157cf82a0eab296f64bc2f012fe92581ed3e3e54e319") ;  
   ADD_CHECKPOINT(478778, "776ae73e23600e0495a9cdcf198095a614c98e1e9ea0d7d80921ffa2bb4709f0") ;
   ADD_CHECKPOINT(478865, "d0351a92a966baa632f4983d5c0f54c1a3c6a52514770f872f890446ba000000") ;
 
    return true;
  }

  bool checkpoints::load_checkpoints_from_json(const std::string &json_hashfile_fullpath)
  {
    boost::system::error_code errcode;
    if (! (boost::filesystem::exists(json_hashfile_fullpath, errcode)))
    {
      LOG_PRINT_L1("Blockchain checkpoints file not found");
      return true;
    }

    LOG_PRINT_L1("Adding checkpoints from blockchain hashfile");

    uint64_t prev_max_height = get_max_height();
    LOG_PRINT_L1("Hard-coded max checkpoint height is " << prev_max_height);
    t_hash_json hashes;
    if (!epee::serialization::load_t_from_json_file(hashes, json_hashfile_fullpath))
    {
      MERROR("Error loading checkpoints from " << json_hashfile_fullpath);
      return false;
    }
    for (std::vector<t_hashline>::const_iterator it = hashes.hashlines.begin(); it != hashes.hashlines.end(); )
    {
      uint64_t height;
      height = it->height;
      if (height <= prev_max_height) {
	LOG_PRINT_L1("ignoring checkpoint height " << height);
      } else {
	std::string blockhash = it->hash;
	LOG_PRINT_L1("Adding checkpoint height " << height << ", hash=" << blockhash);
	ADD_CHECKPOINT(height, blockhash);
      }
      ++it;
    }

    return true;
  }

  bool checkpoints::load_checkpoints_from_dns(network_type nettype)
  {
    std::vector<std::string> records;

    // All four DinastycoinPulse domains have DNSSEC on and valid
    static const std::vector<std::string> dns_urls = {
};


    static const std::vector<std::string> testnet_dns_urls = {
};


    static const std::vector<std::string> stagenet_dns_urls = { "stagenetpoints1.dinastycoin.com"
                   , "stagenetpoints2.dinastycoin.com"
                   , "stagenetpoints3.dinastycoin.com"
                   , "stagenetpoints4.dinastycoin.com"
    };

    if (!tools::dns_utils::load_txt_records_from_dns(records, nettype == TESTNET ? testnet_dns_urls : nettype == STAGENET ? stagenet_dns_urls : dns_urls))
      return true; // why true ?

    for (const auto& record : records)
    {
      auto pos = record.find(":");
      if (pos != std::string::npos)
      {
        uint64_t height;
        crypto::hash hash;

        // parse the first part as uint64_t,
        // if this fails move on to the next record
        std::stringstream ss(record.substr(0, pos));
        if (!(ss >> height))
        {
    continue;
        }

        // parse the second part as crypto::hash,
        // if this fails move on to the next record
        std::string hashStr = record.substr(pos + 1);
        if (!epee::string_tools::hex_to_pod(hashStr, hash))
        {
    continue;
        }

        ADD_CHECKPOINT(height, hashStr);
      }
    }
    return true;
  }

  bool checkpoints::load_new_checkpoints(const std::string &json_hashfile_fullpath, network_type nettype, bool dns)
  {
    bool result;

    result = load_checkpoints_from_json(json_hashfile_fullpath);
    if (dns)
    {
      result &= load_checkpoints_from_dns(nettype);
    }

    return result;
  }
}
