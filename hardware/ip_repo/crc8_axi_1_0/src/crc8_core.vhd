library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity crc8_core is
    Port (
        clk      : in  std_logic;
        reset    : in  std_logic;
        init     : in  std_logic;
        start    : in  std_logic;
        data_in  : in  std_logic_vector(7 downto 0);

        busy     : out std_logic;
        done     : out std_logic;
        crc_out  : out std_logic_vector(7 downto 0)
    );
end crc8_core;

architecture Behavioral of crc8_core is

    signal crc_reg   : std_logic_vector(7 downto 0) := (others => '0');
    signal data_reg  : std_logic_vector(7 downto 0) := (others => '0');
    signal bit_count : integer range 0 to 7 := 0;

    signal busy_reg  : std_logic := '0';
    signal done_reg  : std_logic := '0';

begin

    busy    <= busy_reg;
    done    <= done_reg;
    crc_out <= crc_reg;

    process(clk)
        variable feedback : std_logic;
        variable next_crc : std_logic_vector(7 downto 0);
    begin
        if rising_edge(clk) then

            -- Default done pulse
            done_reg <= '0';

            if reset = '1' then

                crc_reg   <= (others => '0');
                data_reg  <= (others => '0');
                bit_count <= 0;
                busy_reg  <= '0';
                done_reg  <= '0';

            elsif init = '1' and busy_reg = '0' then

                -- Initialize CRC state
                crc_reg <= (others => '0');

            elsif start = '1' and busy_reg = '0' then

                -- Latch one input byte
                data_reg  <= data_in;
                bit_count <= 0;
                busy_reg  <= '1';

            elsif busy_reg = '1' then

                -- Process one input bit per clock, MSB first
                feedback := crc_reg(7) xor data_reg(7);

                next_crc := crc_reg(6 downto 0) & '0';

                if feedback = '1' then
                    next_crc := next_crc xor x"07";
                end if;

                crc_reg  <= next_crc;
                data_reg <= data_reg(6 downto 0) & '0';

                if bit_count = 7 then
                    busy_reg <= '0';
                    done_reg <= '1';
                else
                    bit_count <= bit_count + 1;
                end if;

            end if;

        end if;
    end process;

end Behavioral;